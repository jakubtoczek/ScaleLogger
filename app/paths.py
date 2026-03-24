from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import sys

from .config import AppConfig
from .version import APP_CONFIG_FILENAME, APP_LOGS_DIRNAME, APP_NAME, APP_PRESETS_DIRNAME


@dataclass(slots=True)
class AppPaths:
    data_dir: Path
    bundle_dir: Path
    executable_dir: Path
    config_path: Path

    @classmethod
    def discover(cls) -> "AppPaths":
        source_dir = Path(__file__).resolve().parent.parent
        bundle_dir = Path(getattr(sys, "_MEIPASS", source_dir)).resolve()
        executable_dir = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else source_dir
        data_dir = (Path.home() / APP_NAME).resolve(strict=False)
        return cls(
            data_dir=data_dir,
            bundle_dir=bundle_dir,
            executable_dir=executable_dir,
            config_path=data_dir / APP_CONFIG_FILENAME,
        )

    def resolve_user_path(self, raw_path: str, default_name: str) -> Path:
        candidate = Path(raw_path).expanduser() if raw_path else Path(default_name)
        if candidate.is_absolute():
            try:
                candidate = candidate.resolve(strict=False).relative_to(self.data_dir)
            except ValueError:
                candidate = Path(candidate.name or default_name)

        safe_parts = [part for part in candidate.parts if part not in {"", ".", ".."}]
        relative = Path(*safe_parts) if safe_parts else Path(default_name)
        return (self.data_dir / relative).resolve(strict=False)

    def presets_dir(self, config: AppConfig) -> Path:
        return self.resolve_user_path(config.presets_folder, APP_PRESETS_DIRNAME)

    def logs_dir(self, config: AppConfig) -> Path:
        return self.resolve_user_path(config.logs_folder, APP_LOGS_DIRNAME)

    def ensure_default_storage(self) -> list[str]:
        created: list[str] = []
        for path in (self.data_dir, self.data_dir / APP_LOGS_DIRNAME, self.data_dir / APP_PRESETS_DIRNAME):
            if not path.exists():
                path.mkdir(parents=True, exist_ok=True)
                created.append(str(path))
        return created

    def ensure_runtime_dirs(self, config: AppConfig) -> list[str]:
        created: list[str] = []
        for path in (self.data_dir, self.logs_dir(config), self.presets_dir(config)):
            if not path.exists():
                path.mkdir(parents=True, exist_ok=True)
                created.append(str(path))
        return created

    def icon_candidates(self) -> list[Path]:
        source_dir = Path(__file__).resolve().parent.parent
        candidates = [
            source_dir / "icon.ico",
            self.bundle_dir / "icon.ico",
            self.executable_dir / "icon.ico",
        ]
        unique: list[Path] = []
        seen: set[str] = set()
        for candidate in candidates:
            key = str(candidate)
            if key not in seen:
                seen.add(key)
                unique.append(candidate)
        return unique

    def runtime_icon_path(self) -> Path | None:
        for candidate in self.icon_candidates():
            if candidate.exists():
                return candidate
        return None
