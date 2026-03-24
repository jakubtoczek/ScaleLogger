from __future__ import annotations

import ctypes
import platform
import sys

from PySide6.QtGui import QIcon
from PySide6.QtWidgets import QApplication

from app.paths import AppPaths
from app.ui.main_window import MainWindow
from app.ui.theme import DARK_STYLESHEET
from app.version import APP_ID, APP_NAME, APP_VERSION


def _apply_windows_app_id() -> None:
    if platform.system().lower() != "windows":
        return
    try:
        ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(APP_ID)
    except Exception:
        return


def _load_runtime_icon(paths: AppPaths) -> QIcon | None:
    icon_path = paths.runtime_icon_path()
    if icon_path is None:
        return None
    icon = QIcon(str(icon_path))
    return None if icon.isNull() else icon


def main() -> int:
    paths = AppPaths.discover()
    _apply_windows_app_id()
    app = QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    app.setApplicationVersion(APP_VERSION)
    app.setOrganizationName(APP_NAME)
    app.setDesktopFileName(APP_ID)
    app.setStyleSheet(DARK_STYLESHEET)

    icon = _load_runtime_icon(paths)
    if icon is not None:
        app.setWindowIcon(icon)

    window = MainWindow(paths)
    if icon is not None:
        window.setWindowIcon(icon)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
