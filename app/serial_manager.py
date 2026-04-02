from __future__ import annotations

from dataclasses import replace
import os
import time

from PySide6.QtCore import QObject, QThread, QTimer, Signal
import serial

from .config import SerialSettings, normalize_serial_port_name

BYTESIZE_MAP = {
    5: serial.FIVEBITS,
    6: serial.SIXBITS,
    7: serial.SEVENBITS,
    8: serial.EIGHTBITS,
}
PARITY_MAP = {
    "N": serial.PARITY_NONE,
    "E": serial.PARITY_EVEN,
    "O": serial.PARITY_ODD,
    "M": serial.PARITY_MARK,
    "S": serial.PARITY_SPACE,
}
STOPBITS_MAP = {
    1.0: serial.STOPBITS_ONE,
    1.5: serial.STOPBITS_ONE_POINT_FIVE,
    2.0: serial.STOPBITS_TWO,
}


def discard_open_stale_input(port: serial.Serial, settle_seconds: float = 0.1, drain_passes: int = 4) -> int:
    port.reset_input_buffer()
    time.sleep(max(0.0, settle_seconds))
    discarded = b""
    for _ in range(max(1, drain_passes)):
        waiting = int(getattr(port, "in_waiting", 0) or 0)
        if waiting <= 0:
            break
        if hasattr(port, "read_all"):
            discarded += port.read_all() or b""
        else:
            discarded += port.read(waiting) or b""
        time.sleep(min(max(0.0, settle_seconds), 0.05))
    port.reset_input_buffer()
    return len(discarded)


class SerialWorker(QObject):
    line_received = Signal(str)
    info = Signal(str)
    error = Signal(str)
    connected = Signal()
    disconnected = Signal()

    def __init__(self, settings: SerialSettings) -> None:
        super().__init__()
        self._settings = settings
        self._running = True
        self._serial: serial.Serial | None = None

    def run(self) -> None:
        try:
            self._serial = serial.Serial(
                port=self._settings.port,
                baudrate=self._settings.baudrate,
                bytesize=BYTESIZE_MAP[self._settings.databits],
                parity=PARITY_MAP[self._settings.parity],
                stopbits=STOPBITS_MAP[self._settings.stopbits],
                timeout=self._settings.timeout,
            )
            discarded_bytes = discard_open_stale_input(self._serial)
            self.connected.emit()
            self.info.emit(
                f"Serial connection opened on {self._settings.port} at {self._settings.baudrate} baud."
            )
            if discarded_bytes:
                self.info.emit("Discarded buffered serial data on connect.")

            delimiter = self._settings.eol.encode("utf-8")
            while self._running:
                try:
                    chunk = self._serial.read_until(expected=delimiter)
                except serial.SerialException as exc:
                    self.error.emit(f"Serial read error: {exc}")
                    break

                if not self._running:
                    break
                if not chunk:
                    continue

                decoded = chunk.decode("utf-8", errors="replace")
                if "\ufffd" in decoded:
                    self.info.emit("Decode issue detected; undecodable bytes were replaced.")
                if decoded.endswith(self._settings.eol):
                    decoded = decoded[: -len(self._settings.eol)]
                self.line_received.emit(decoded)
        except serial.SerialException as exc:
            self.error.emit(f"Serial connection failed: {exc}")
        except Exception as exc:  # pragma: no cover - defensive around device I/O
            self.error.emit(f"Unexpected serial worker error: {exc}")
        finally:
            self._close_port()
            self.disconnected.emit()

    def stop(self) -> None:
        self._running = False
        self._close_port()

    def _close_port(self) -> None:
        if self._serial is not None:
            try:
                self._serial.close()
            except Exception:
                pass
            self._serial = None


class SerialManager(QObject):
    line_received = Signal(str)
    info = Signal(str)
    error = Signal(str)
    state_changed = Signal(str)

    def __init__(self) -> None:
        super().__init__()
        self._thread: QThread | None = None
        self._worker: SerialWorker | None = None
        self._state = "disconnected"
        self._forced_no_serial = False

    def connect_port(self, settings: SerialSettings) -> None:
        self.disconnect_port(wait_ms=2000)
        self._set_state("connecting")
        force_no_serial = os.getenv("SCALELOGGER_FORCE_NO_SERIAL", "").strip().lower()
        if force_no_serial in {"1", "true", "yes", "on"}:
            self._forced_no_serial = True
            self.info.emit("SCALELOGGER_FORCE_NO_SERIAL is enabled; skipping serial device open.")
            QTimer.singleShot(10, lambda: self._set_state("connected"))
            return

        self._forced_no_serial = False
        normalized_settings = replace(settings, port=normalize_serial_port_name(settings.port, system_name="Windows"))

        self._thread = QThread()
        self._worker = SerialWorker(normalized_settings)
        self._worker.moveToThread(self._thread)

        self._thread.started.connect(self._worker.run)
        self._worker.line_received.connect(self.line_received)
        self._worker.info.connect(self.info)
        self._worker.error.connect(self._handle_error)
        self._worker.connected.connect(lambda: self._set_state("connected"))
        self._worker.disconnected.connect(self._handle_disconnected)
        self._worker.disconnected.connect(self._thread.quit)
        self._thread.finished.connect(self._cleanup)
        self._thread.start()

    def disconnect_port(self, wait_ms: int = 1500) -> None:
        if self._forced_no_serial:
            self._forced_no_serial = False
            self._set_state("disconnected")
            return
        if self._worker is not None:
            self._worker.stop()
        if self._thread is not None:
            self._thread.quit()
            self._thread.wait(wait_ms)

    def is_connected(self) -> bool:
        return self._state == "connected"

    def _handle_error(self, message: str) -> None:
        self.error.emit(message)
        self._set_state("error")

    def _handle_disconnected(self) -> None:
        if self._state != "error":
            self._set_state("disconnected")

    def _cleanup(self) -> None:
        self._worker = None
        self._thread = None
        if self._state not in {"disconnected", "error"}:
            self._set_state("disconnected")

    def _set_state(self, state: str) -> None:
        self._state = state
        self.state_changed.emit(state)
