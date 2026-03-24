from __future__ import annotations

from dataclasses import dataclass
import json
from pathlib import Path

from .config import AppConfig, AppSettings


@dataclass(slots=True)
class PresetRecord:
    name: str
    path: Path
    settings: AppSettings


@dataclass(slots=True)
class PresetDiscovery:
    records: list[PresetRecord]
    warnings: list[str]


PRESET_SUFFIX = ".json"
PRESET_FIELDS = {
    "name",
    "port",
    "baudrate",
    "databits",
    "parity",
    "stopbits",
    "timeout",
    "eol",
    "mode",
    "trim_whitespace",
    "strip_suffix",
    "suffix",
    "normalize_sign",
    "drop_plus_sign",
    "numeric_validation",
    "post_action",
    "custom_sequence",
}
REQUIRED_PRESET_FIELDS = PRESET_FIELDS - {"name", "custom_sequence"}


def discover_presets(folder: Path) -> PresetDiscovery:
    records: list[PresetRecord] = []
    warnings: list[str] = []
    seen_names: set[str] = set()

    if not folder.exists():
        warnings.append(f"No user presets found yet. The presets folder will be created at {folder}.")
        return PresetDiscovery(records=records, warnings=warnings)

    if not folder.is_dir():
        warnings.append(f"Presets path is not a folder: {folder}")
        return PresetDiscovery(records=records, warnings=warnings)

    for path in sorted(folder.glob(f"*{PRESET_SUFFIX}")):
        try:
            record = load_preset(path)
        except Exception as exc:
            warnings.append(f"Skipping preset '{path.name}': {exc}")
            continue
        if record.name in seen_names:
            warnings.append(f"Skipping preset '{path.name}': duplicate preset name '{record.name}'")
            continue
        seen_names.add(record.name)
        records.append(record)

    return PresetDiscovery(records=records, warnings=warnings)


def resolve_startup_preset_name(config: AppConfig, available_names: set[str]) -> str | None:
    requested_name = ""
    if config.startup_mode == "specific_preset":
        requested_name = config.startup_preset_name
    elif config.startup_mode == "last_used_preset":
        requested_name = config.last_used_preset_name

    if not requested_name:
        return None
    return requested_name if requested_name in available_names else None


def load_preset(path: Path) -> PresetRecord:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise ValueError("preset root must be a JSON object")
    if any(key in payload for key in ("serial", "parsing", "output")):
        raise ValueError("legacy nested preset format is not supported")

    missing_keys = sorted(REQUIRED_PRESET_FIELDS - payload.keys())
    if missing_keys:
        raise ValueError(f"missing required preset keys: {', '.join(missing_keys)}")

    unexpected_keys = sorted(payload.keys() - PRESET_FIELDS)
    if unexpected_keys:
        raise ValueError(f"unexpected preset keys: {', '.join(unexpected_keys)}")

    name = str(payload.get("name") or path.stem).strip() or path.stem
    settings = AppSettings.from_dict(_flat_to_nested_settings(payload))
    return PresetRecord(name=name, path=path, settings=settings)


def save_preset(path: Path, name: str, settings: AppSettings) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "name": name,
        "port": settings.serial.port,
        "baudrate": settings.serial.baudrate,
        "databits": settings.serial.databits,
        "parity": settings.serial.parity,
        "stopbits": settings.serial.stopbits,
        "timeout": settings.serial.timeout,
        "eol": settings.serial.eol.encode("unicode_escape").decode("ascii"),
        "mode": settings.parsing.mode,
        "trim_whitespace": settings.parsing.trim_whitespace,
        "strip_suffix": settings.parsing.strip_suffix,
        "suffix": settings.parsing.suffix,
        "normalize_sign": settings.parsing.normalize_sign,
        "drop_plus_sign": settings.parsing.drop_plus_sign,
        "numeric_validation": settings.parsing.numeric_validation,
        "post_action": settings.output.post_action,
        "custom_sequence": settings.output.custom_sequence,
    }
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _flat_to_nested_settings(payload: dict[str, object]) -> dict[str, object]:
    return {
        "serial": {
            "port": payload["port"],
            "baudrate": payload["baudrate"],
            "databits": payload["databits"],
            "parity": payload["parity"],
            "stopbits": payload["stopbits"],
            "timeout": payload["timeout"],
            "eol": payload["eol"],
        },
        "parsing": {
            "mode": payload["mode"],
            "trim_whitespace": payload["trim_whitespace"],
            "strip_suffix": payload["strip_suffix"],
            "suffix": payload["suffix"],
            "normalize_sign": payload["normalize_sign"],
            "drop_plus_sign": payload["drop_plus_sign"],
            "numeric_validation": payload["numeric_validation"],
        },
        "output": {
            "post_action": payload["post_action"],
            "custom_sequence": payload.get("custom_sequence", []),
        },
    }
