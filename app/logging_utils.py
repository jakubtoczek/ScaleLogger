from __future__ import annotations

from datetime import datetime
import logging
import os
from pathlib import Path

LOGGER_NAME = "ScaleLogger"
LOG_FILE_PREFIX = "ScaleLogger"
LOG_FILE_SINGLE = f"{LOG_FILE_PREFIX}.log"


class SessionLogger:
    def __init__(self, logs_dir: Path, mode: str = "per_session") -> None:
        self.logs_dir = logs_dir
        self.mode = mode
        self.path: Path | None = None
        self.logger = logging.getLogger(LOGGER_NAME)
        self.logger.setLevel(logging.INFO)
        self.logger.handlers.clear()
        self.logger.propagate = False
        self._handler: logging.FileHandler | None = None

        if self.mode == "none":
            return

        self.logs_dir.mkdir(parents=True, exist_ok=True)
        self.path = self._resolve_log_path()

        formatter = logging.Formatter("%(asctime)s | %(levelname)s | %(message)s", "%Y-%m-%d %H:%M:%S")
        self._handler = logging.FileHandler(self.path, encoding="utf-8", mode="a")
        self._handler.setFormatter(formatter)
        self.logger.addHandler(self._handler)

    def write(self, level: str, message: str) -> None:
        if self._handler is None:
            return
        log_method = getattr(self.logger, level, self.logger.info)
        log_method(message)
        self._flush_handler()

    def close(self) -> None:
        if self._handler is None:
            return
        self._flush_handler()
        self._handler.close()
        self.logger.handlers.clear()
        self._handler = None

    def _resolve_log_path(self) -> Path:
        if self.mode == "single_file":
            return self.logs_dir / LOG_FILE_SINGLE
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        return self.logs_dir / f"{LOG_FILE_PREFIX}_{timestamp}.log"

    def _flush_handler(self) -> None:
        if self._handler is None:
            return
        self._handler.flush()
        stream = getattr(self._handler, "stream", None)
        if stream is None:
            return
        try:
            os.fsync(stream.fileno())
        except OSError:
            pass
