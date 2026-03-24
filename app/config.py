from __future__ import annotations

from dataclasses import asdict, dataclass, field
import json
import platform
from pathlib import Path
from typing import Any

from .version import APP_LOGS_DIRNAME, APP_PRESETS_DIRNAME
from .key_sequences import CUSTOM_SEQUENCE_ACTION, normalize_key_token

STARTUP_MODES = ("built_in_defaults", "specific_preset", "last_used_preset")
PARSE_MODES = ("parsed", "raw")
POST_ACTIONS = ("down", "right", "enter", "tab", "none", CUSTOM_SEQUENCE_ACTION)
LOG_MODES = ("none", "single_file", "per_session")
LINE_LOG_MODES = ("compact", "verbose")
PARITY_VALUES = ("N", "O", "E", "M", "S")
PARITY_LABELS = {
    "N": "N (None)",
    "O": "O (Odd)",
    "E": "E (Even)",
    "M": "M (Mark)",
    "S": "S (Space)",
}
BAUDRATE_PRESETS = ("110", "300", "600", "1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200")
TIMEOUT_PRESETS = ("0.10", "0.25", "0.50", "0.80", "1.00", "2.00", "5.00")
DATA_BITS = (5, 6, 7, 8)
STOP_BITS = (1.0, 1.5, 2.0)


@dataclass(slots=True)
class SerialSettings:
    port: str = "COM6"
    baudrate: int = 1200
    databits: int = 7
    parity: str = "O"
    stopbits: float = 1.0
    timeout: float = 1.0
    eol: str = "\r\n"


@dataclass(slots=True)
class ParsingSettings:
    mode: str = "parsed"
    trim_whitespace: bool = True
    strip_suffix: bool = True
    suffix: str = "g"
    normalize_sign: bool = True
    drop_plus_sign: bool = False
    numeric_validation: bool = True


@dataclass(slots=True)
class OutputSettings:
    post_action: str = "down"
    custom_sequence: list[str] = field(default_factory=list)


@dataclass(slots=True)
class AppSettings:
    serial: SerialSettings
    parsing: ParsingSettings
    output: OutputSettings

    @classmethod
    def built_in_defaults(cls) -> "AppSettings":
        return cls(serial=SerialSettings(), parsing=ParsingSettings(), output=OutputSettings())

    def clone(self) -> "AppSettings":
        return AppSettings.from_dict(self.to_dict())

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    @classmethod
    def from_dict(cls, payload: dict[str, Any]) -> "AppSettings":
        settings = cls.built_in_defaults()

        serial_payload = payload.get("serial", {}) if isinstance(payload.get("serial"), dict) else {}
        settings.serial.port = normalize_serial_port_name(serial_payload.get("port", settings.serial.port), system_name="Windows")
        settings.serial.baudrate = _to_int(serial_payload.get("baudrate"), settings.serial.baudrate)
        settings.serial.databits = _to_int(serial_payload.get("databits"), settings.serial.databits)
        settings.serial.parity = normalize_parity_value(serial_payload.get("parity"), settings.serial.parity)
        settings.serial.stopbits = _to_float(serial_payload.get("stopbits"), settings.serial.stopbits)
        settings.serial.timeout = _to_float(serial_payload.get("timeout"), settings.serial.timeout)
        settings.serial.eol = _normalize_eol(serial_payload.get("eol"), settings.serial.eol)

        parsing_payload = payload.get("parsing", {}) if isinstance(payload.get("parsing"), dict) else {}
        settings.parsing.mode = _to_choice(parsing_payload.get("mode"), PARSE_MODES, settings.parsing.mode)
        settings.parsing.trim_whitespace = _to_bool(parsing_payload.get("trim_whitespace"), settings.parsing.trim_whitespace)
        settings.parsing.strip_suffix = _to_bool(parsing_payload.get("strip_suffix"), settings.parsing.strip_suffix)
        settings.parsing.suffix = str(parsing_payload.get("suffix", settings.parsing.suffix))
        settings.parsing.normalize_sign = _to_bool(parsing_payload.get("normalize_sign"), settings.parsing.normalize_sign)
        settings.parsing.drop_plus_sign = _to_bool(parsing_payload.get("drop_plus_sign"), settings.parsing.drop_plus_sign)
        settings.parsing.numeric_validation = _to_bool(
            parsing_payload.get("numeric_validation"), settings.parsing.numeric_validation
        )

        output_payload = payload.get("output", {}) if isinstance(payload.get("output"), dict) else {}
        settings.output.post_action = _to_choice(output_payload.get("post_action"), POST_ACTIONS, settings.output.post_action)
        settings.output.custom_sequence = _normalize_key_sequence(
            output_payload.get("custom_sequence"), settings.output.custom_sequence or []
        )

        settings.validate()
        return settings

    def validate(self) -> None:
        if self.serial.databits not in DATA_BITS:
            raise ValueError("Serial data bits must be one of 5, 6, 7, or 8.")
        if self.serial.parity not in PARITY_VALUES:
            raise ValueError("Serial parity must be one of N, O, E, M, or S.")
        if self.serial.stopbits not in STOP_BITS:
            raise ValueError("Serial stop bits must be 1, 1.5, or 2.")
        if self.serial.baudrate <= 0:
            raise ValueError("Serial baud rate must be greater than zero.")
        if self.serial.timeout < 0:
            raise ValueError("Serial timeout must be zero or greater.")
        if self.serial.eol == "":
            raise ValueError("Serial line ending must not be empty.")
        if self.parsing.mode not in PARSE_MODES:
            raise ValueError("Parsing mode must be 'parsed' or 'raw'.")
        if self.output.post_action not in POST_ACTIONS:
            raise ValueError("Post-action key is invalid.")
        if self.output.post_action == CUSTOM_SEQUENCE_ACTION and not self.output.custom_sequence:
            raise ValueError("Custom after-send sequence must contain at least one key.")


@dataclass(slots=True)
class AppConfig:
    presets_folder: str = APP_PRESETS_DIRNAME
    logs_folder: str = APP_LOGS_DIRNAME
    log_mode: str = "per_session"
    line_log_mode: str = "compact"
    connect_on_startup: bool = True
    startup_mode: str = "last_used_preset"
    startup_preset_name: str = ""
    last_used_preset_name: str = ""

    @classmethod
    def built_in_defaults(cls) -> "AppConfig":
        return cls()

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    @classmethod
    def from_dict(cls, payload: dict[str, Any]) -> "AppConfig":
        config = cls.built_in_defaults()
        config.presets_folder = str(payload.get("presets_folder", config.presets_folder)).strip() or config.presets_folder
        config.logs_folder = str(payload.get("logs_folder", config.logs_folder)).strip() or config.logs_folder
        config.log_mode = _to_choice(payload.get("log_mode"), LOG_MODES, config.log_mode)
        config.line_log_mode = _to_choice(payload.get("line_log_mode"), LINE_LOG_MODES, config.line_log_mode)
        config.connect_on_startup = _to_bool(payload.get("connect_on_startup"), config.connect_on_startup)
        config.startup_mode = _to_choice(payload.get("startup_mode"), STARTUP_MODES, config.startup_mode)
        config.startup_preset_name = str(payload.get("startup_preset_name", "")).strip()
        config.last_used_preset_name = str(payload.get("last_used_preset_name", "")).strip()
        config.validate()
        return config

    def validate(self) -> None:
        if self.log_mode not in LOG_MODES:
            raise ValueError("Log mode is invalid.")
        if self.line_log_mode not in LINE_LOG_MODES:
            raise ValueError("Line log mode is invalid.")
        if self.startup_mode not in STARTUP_MODES:
            raise ValueError("Startup mode is invalid.")


def load_app_config(path: Path) -> AppConfig:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise ValueError("ScaleLogger.config.json must contain a JSON object.")
    return AppConfig.from_dict(payload)


def save_app_config(path: Path, config: AppConfig) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(config.to_dict(), indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _to_int(value: Any, fallback: int) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return fallback


def _to_float(value: Any, fallback: float) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return fallback


def _to_bool(value: Any, fallback: bool) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        lowered = value.strip().lower()
        if lowered in {"1", "true", "yes", "y", "on"}:
            return True
        if lowered in {"0", "false", "no", "n", "off"}:
            return False
    return fallback


def _to_choice(value: Any, options: tuple[str, ...], fallback: str) -> str:
    if isinstance(value, str):
        candidate = value.strip().lower()
        if candidate in options:
            return candidate
    return fallback


def _normalize_eol(value: Any, fallback: str) -> str:
    if not isinstance(value, str) or not value:
        return fallback
    normalized = value.replace("\\r", "\r").replace("\\n", "\n")
    return normalized


def normalize_serial_port_name(value: Any, system_name: str | None = None) -> str:
    candidate = str(value).strip() if value is not None else ""
    if not candidate:
        return ""

    normalized_system = (system_name or platform.system()).strip().lower()
    if normalized_system == "windows":
        upper = candidate.upper()
        if candidate.isdigit():
            return f"COM{int(candidate)}"
        if upper.startswith("COM") and upper[3:].isdigit():
            return f"COM{int(upper[3:])}"
    return candidate


def normalize_parity_value(value: Any, fallback: str = "N") -> str:
    candidate = str(value).strip().lower() if value is not None else ""
    mapping = {
        "n": "N",
        "none": "N",
        "o": "O",
        "odd": "O",
        "e": "E",
        "even": "E",
        "m": "M",
        "mark": "M",
        "s": "S",
        "space": "S",
    }
    if candidate and candidate[0] in mapping and candidate.startswith(f"{candidate[0]} "):
        candidate = candidate[0]
    if candidate and candidate[0] in mapping and candidate.startswith(f"{candidate[0]}("):
        candidate = candidate[0]
    return mapping.get(candidate, fallback)


def _normalize_key_sequence(value: Any, fallback: list[str]) -> list[str]:
    if not isinstance(value, list):
        return list(fallback)
    normalized = [token for token in (normalize_key_token(str(item)) for item in value) if token]
    return normalized


def serial_settings_require_reconnect(current: SerialSettings, updated: SerialSettings) -> bool:
    return any(
        (
            normalize_serial_port_name(current.port, system_name="Windows")
            != normalize_serial_port_name(updated.port, system_name="Windows"),
            current.baudrate != updated.baudrate,
            current.databits != updated.databits,
            current.parity != updated.parity,
            current.stopbits != updated.stopbits,
            current.timeout != updated.timeout,
            current.eol != updated.eol,
        )
    )
