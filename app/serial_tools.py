from __future__ import annotations

from dataclasses import dataclass
import re

from serial.tools import list_ports

from .config import PARITY_LABELS, normalize_parity_value, normalize_serial_port_name

_PORT_SPLIT_PATTERN = re.compile(r"\s+[—-]\s+")


@dataclass(slots=True)
class SerialPortEntry:
    port: str
    description: str = ""
    manufacturer: str = ""
    product: str = ""

    @property
    def label(self) -> str:
        extras = [part for part in (self.description, self.manufacturer, self.product) if part]
        if not extras:
            return self.port
        summary = " | ".join(dict.fromkeys(extras))
        return f"{self.port} — {summary}"


COMMON_PORT_HINT = "Scan to list available serial ports, or type a port name manually."


def list_available_serial_ports() -> list[SerialPortEntry]:
    entries: list[SerialPortEntry] = []
    for port_info in sorted(list_ports.comports(), key=lambda item: _port_sort_key(item.device)):
        entries.append(
            SerialPortEntry(
                port=normalize_serial_port_name(port_info.device, system_name="Windows"),
                description=(port_info.description or "").strip(),
                manufacturer=(port_info.manufacturer or "").strip(),
                product=(port_info.product or "").strip(),
            )
        )
    return entries


def port_value_from_label(text: str) -> str:
    candidate = (text or "").strip()
    if not candidate:
        return ""
    parts = _PORT_SPLIT_PATTERN.split(candidate, maxsplit=1)
    return normalize_serial_port_name(parts[0], system_name="Windows")


def parity_label_for_value(value: str) -> str:
    normalized = normalize_parity_value(value, "N")
    return PARITY_LABELS.get(normalized, normalized)


def _port_sort_key(port_name: str) -> tuple[int, int | str]:
    normalized = normalize_serial_port_name(port_name, system_name="Windows")
    upper = normalized.upper()
    if upper.startswith("COM") and upper[3:].isdigit():
        return (0, int(upper[3:]))
    return (1, upper)
