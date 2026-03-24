from __future__ import annotations

from collections.abc import Callable
from datetime import datetime

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QDialog, QHBoxLayout, QLabel, QPushButton, QTextEdit, QVBoxLayout

from ..config import SerialSettings
from ..serial_manager import SerialManager


class TestReceiveDialog(QDialog):
    def __init__(
        self,
        settings: SerialSettings,
        port_in_use: bool,
        log_callback: Callable[[str, str], None] | None = None,
        parent=None,
    ) -> None:
        super().__init__(parent)
        self.setWindowTitle("Test Receive")
        self.resize(560, 340)
        self._settings = settings
        self._port_in_use = port_in_use
        self._log_callback = log_callback
        self._serial = SerialManager()
        self._received_any = False
        self._status_timer = QTimer(self)
        self._status_timer.setSingleShot(True)
        self._status_timer.timeout.connect(self._mark_no_data_yet)
        self._build_ui()
        self._connect_signals()
        self._set_status(
            "Main capture is active. Stop it before testing."
            if self._port_in_use
            else "Ready"
        )
        self.start_button.setEnabled(not self._port_in_use)
        self.stop_button.setEnabled(False)

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)

        self.summary_label = QLabel(self._summary_text())
        self.summary_label.setWordWrap(True)
        layout.addWidget(self.summary_label)

        self.status_label = QLabel()
        self.status_label.setWordWrap(True)
        layout.addWidget(self.status_label)

        controls = QHBoxLayout()
        self.start_button = QPushButton("Start")
        self.stop_button = QPushButton("Stop")
        close_button = QPushButton("Close")
        close_button.clicked.connect(self.close)
        controls.addWidget(self.start_button)
        controls.addWidget(self.stop_button)
        controls.addStretch(1)
        controls.addWidget(close_button)
        layout.addLayout(controls)

        self.log_view = QTextEdit()
        self.log_view.setReadOnly(True)
        layout.addWidget(self.log_view)

    def _connect_signals(self) -> None:
        self.start_button.clicked.connect(self._start_monitoring)
        self.stop_button.clicked.connect(self._stop_monitoring)
        self._serial.line_received.connect(self._on_line_received)
        self._serial.info.connect(self._on_info)
        self._serial.error.connect(self._on_error)
        self._serial.state_changed.connect(self._on_state_changed)

    def _summary_text(self) -> str:
        return (
            f"Port {self._settings.port} | {self._settings.baudrate} baud | {self._settings.databits}{self._settings.parity}{self._settings.stopbits:g} | "
            f"Timeout {self._settings.timeout:.2f}s"
        )

    def _start_monitoring(self) -> None:
        if self._port_in_use:
            self._set_status("Main capture is active. Stop it before testing.")
            return
        self._received_any = False
        self.log_view.clear()
        self._set_status("Waiting for data…")
        self._status_timer.start(3000)
        self._serial.connect_port(self._settings)
        self.start_button.setEnabled(False)
        self.stop_button.setEnabled(True)
        self._log_external("info", f"Test Receive started on {self._settings.port}")

    def _stop_monitoring(self) -> None:
        self._status_timer.stop()
        self._serial.disconnect_port(wait_ms=2000)
        self.start_button.setEnabled(not self._port_in_use)
        self.stop_button.setEnabled(False)
        if self._received_any:
            self._set_status("Stopped")
        else:
            self._set_status("Stopped — no data")
            self._log_external("info", "No data received during test")
        self._log_external("info", "Test Receive stopped")

    def _mark_no_data_yet(self) -> None:
        if not self._received_any and not self.start_button.isEnabled():
            self._set_status("No data yet")

    def _on_state_changed(self, state: str) -> None:
        if state == "connected":
            self._append_log(f"Connected to {self._settings.port}")
        elif state == "connecting":
            self._append_log(f"Opening {self._settings.port}…")

    def _on_line_received(self, raw_line: str) -> None:
        self._received_any = True
        self._status_timer.stop()
        self._set_status("Receiving data")
        self._append_log(f"Raw: {raw_line!r}")

    def _on_info(self, message: str) -> None:
        if "Decode issue" in message:
            self._set_status("Decode issue")
        self._append_log(message)

    def _on_error(self, message: str) -> None:
        self._status_timer.stop()
        self._set_status("Serial error")
        self._append_log(message)
        self._log_external("warning", f"Test Receive error: {message}")
        self.start_button.setEnabled(not self._port_in_use)
        self.stop_button.setEnabled(False)

    def _append_log(self, message: str) -> None:
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_view.append(f"[{timestamp}] {message}")

    def _set_status(self, message: str) -> None:
        self.status_label.setText(message)

    def _log_external(self, level: str, message: str) -> None:
        if self._log_callback is not None:
            self._log_callback(level, message)

    def closeEvent(self, event) -> None:  # noqa: N802
        self._status_timer.stop()
        self._serial.disconnect_port(wait_ms=1000)
        super().closeEvent(event)
