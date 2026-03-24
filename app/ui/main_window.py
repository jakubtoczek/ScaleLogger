from __future__ import annotations

from datetime import datetime
import logging
import sys
import traceback

from PySide6.QtCore import QTimer
from PySide6.QtGui import QTextCursor
from PySide6.QtWidgets import (
    QComboBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QTextEdit,
    QToolButton,
    QVBoxLayout,
    QWidget,
)

from ..config import AppConfig, AppSettings, load_app_config, save_app_config, serial_settings_require_reconnect
from ..logging_utils import SessionLogger
from ..output_sender import WindowsInputSender
from ..parser import ScaleLineParser
from ..paths import AppPaths
from ..presets import PresetDiscovery, PresetRecord, discover_presets, load_preset, resolve_startup_preset_name
from ..serial_manager import SerialManager
from ..version import APP_NAME, APP_VERSION, APP_WINDOW_TITLE, about_text
from .settings_dialog import SettingsDialog


class MainWindow(QMainWindow):
    def __init__(self, paths: AppPaths) -> None:
        super().__init__()
        self._paths = paths
        self._app_config = AppConfig.built_in_defaults()
        self._settings = AppSettings.built_in_defaults()
        self._preset_records: dict[str, PresetRecord] = {}
        self._active_preset_name: str | None = None
        self._serial = SerialManager()
        self._parser = ScaleLineParser()
        self._sender = WindowsInputSender()
        self._session_logger: SessionLogger | None = None
        self._suppress_preset_change = False
        self._startup_messages: list[tuple[str, str]] = []
        self._original_excepthook = sys.excepthook

        self.setWindowTitle(APP_WINDOW_TITLE)
        self.resize(820, 520)
        self._build_ui()
        self._connect_signals()
        self._bootstrap()

    def _build_ui(self) -> None:
        root = QWidget()
        self.setCentralWidget(root)
        layout = QVBoxLayout(root)
        layout.setSpacing(12)

        toolbar = QHBoxLayout()
        toolbar.setSpacing(8)

        preset_label = QLabel("Presets")
        preset_label.setObjectName("sectionLabel")
        self.preset_combo = QComboBox()
        self.preset_combo.setMinimumWidth(240)
        self.preset_combo.setPlaceholderText("Built-in defaults active")

        self.refresh_presets_button = QToolButton()
        self.refresh_presets_button.setObjectName("toolbarIconButton")
        self.refresh_presets_button.setText("⟳")
        self.refresh_presets_button.setToolTip("Refresh presets")
        self.refresh_presets_button.setAccessibleName("Refresh presets")

        self.status_dot = QLabel("●")
        self.status_dot.setObjectName("statusDot")
        self.status_text = QLabel("Disconnected")

        self.connect_button = QPushButton("Connect")
        self.connect_button.setCheckable(True)

        self.settings_button = QToolButton()
        self.settings_button.setObjectName("toolbarIconButton")
        self.settings_button.setText("⚙")
        self.settings_button.setToolTip("Settings")
        self.settings_button.setAccessibleName("Settings")

        self.about_button = QToolButton()
        self.about_button.setObjectName("toolbarIconButton")
        self.about_button.setText("❔")
        self.about_button.setToolTip("About ScaleLogger")
        self.about_button.setAccessibleName("About ScaleLogger")

        toolbar.addWidget(preset_label)
        toolbar.addWidget(self.preset_combo)
        toolbar.addWidget(self.refresh_presets_button)
        toolbar.addStretch(1)
        toolbar.addWidget(self.status_dot)
        toolbar.addWidget(self.status_text)
        toolbar.addWidget(self.connect_button)
        toolbar.addWidget(self.settings_button)
        toolbar.addWidget(self.about_button)
        layout.addLayout(toolbar)

        self.log_view = QTextEdit()
        self.log_view.setReadOnly(True)
        self.log_view.setObjectName("logView")
        layout.addWidget(self.log_view)

    def _connect_signals(self) -> None:
        self.connect_button.clicked.connect(self._toggle_connection)
        self.settings_button.clicked.connect(self._open_settings)
        self.about_button.clicked.connect(self._show_about_dialog)
        self.refresh_presets_button.clicked.connect(self.refresh_presets)
        self.preset_combo.currentTextChanged.connect(self._on_preset_selected)
        self._serial.state_changed.connect(self._on_state_changed)
        self._serial.line_received.connect(self._on_line_received)
        self._serial.info.connect(lambda message: self._log("info", message))
        self._serial.error.connect(lambda message: self._log("error", message))

    def _bootstrap(self) -> None:
        self._prepare_storage()
        self._load_app_config()
        self._record_created_dirs(self._paths.ensure_runtime_dirs(self._app_config))
        self._reset_session_logger()
        self._install_exception_hook()
        self._log("info", "Application start.")
        self._log("info", f"Version: {APP_VERSION}")
        self._log("info", f"Configuration path: {self._paths.config_path}")
        self._log("info", f"Logs path: {self._paths.logs_dir(self._app_config)}")
        self._log("info", f"Log recording mode: {self._app_config.log_mode}")
        self._log("info", f"Presets path: {self._paths.presets_dir(self._app_config)}")
        for level, message in self._startup_messages:
            self._log(level, message)
        self.refresh_presets()
        self._apply_startup_behavior()
        self._set_status("disconnected")
        QTimer.singleShot(0, self._auto_connect_if_enabled)

    def _prepare_storage(self) -> None:
        self._record_created_dirs(self._paths.ensure_default_storage())

    def _record_created_dirs(self, created_paths: list[str]) -> None:
        for path in created_paths:
            self._startup_messages.append(("info", f"Created application data folder: {path}"))

    def _log_created_dirs(self, created_paths: list[str]) -> None:
        for path in created_paths:
            self._log("info", f"Created application data folder: {path}")

    def _load_app_config(self) -> None:
        if not self._paths.config_path.exists():
            self._app_config = AppConfig.built_in_defaults()
            try:
                save_app_config(self._paths.config_path, self._app_config)
            except Exception as exc:
                self._startup_messages.append(("error", f"Could not create default configuration: {exc}"))
            else:
                self._startup_messages.append(
                    ("info", f"No configuration file found; created default configuration at: {self._paths.config_path}")
                )
            return

        try:
            self._app_config = load_app_config(self._paths.config_path)
        except Exception as exc:
            self._app_config = AppConfig.built_in_defaults()
            self._startup_messages.append(("warning", f"Could not load configuration; using built-in defaults. {exc}"))
            return

        self._startup_messages.append(("info", f"Loaded configuration from: {self._paths.config_path}"))

    def _reset_session_logger(self) -> None:
        if self._session_logger is not None:
            self._session_logger.close()
        self._session_logger = SessionLogger(self._paths.logs_dir(self._app_config), mode=self._app_config.log_mode)

    def _install_exception_hook(self) -> None:
        def _handle_unhandled_exception(exc_type, exc_value, exc_traceback) -> None:
            formatted = "".join(traceback.format_exception(exc_type, exc_value, exc_traceback)).rstrip()
            self._log("error", f"Unhandled exception:\n{formatted}")
            self._original_excepthook(exc_type, exc_value, exc_traceback)

        sys.excepthook = _handle_unhandled_exception

    def refresh_presets(self) -> None:
        discovery = discover_presets(self._paths.presets_dir(self._app_config))
        self._apply_preset_discovery(discovery)
        for warning in discovery.warnings:
            self._log("warning", f"Preset warning: {warning}")

    def _apply_preset_discovery(self, discovery: PresetDiscovery) -> None:
        self._preset_records = {record.name: record for record in discovery.records}
        if self._active_preset_name not in self._preset_records:
            self._active_preset_name = None

        self._suppress_preset_change = True
        try:
            self.preset_combo.clear()
            self.preset_combo.addItems(list(self._preset_records))
            self.preset_combo.setEnabled(bool(self._preset_records))
            self._sync_preset_combo_to_active_state()
        finally:
            self._suppress_preset_change = False

        self.preset_combo.setToolTip("" if self._preset_records else "No user presets found yet.")

    def _sync_preset_combo_to_active_state(self) -> None:
        placeholder = "Built-in defaults active" if self._preset_records else "No user presets yet"
        self.preset_combo.setPlaceholderText(placeholder)
        if self._active_preset_name and self._active_preset_name in self._preset_records:
            self.preset_combo.setCurrentText(self._active_preset_name)
        else:
            self.preset_combo.setCurrentIndex(-1)

    def _apply_startup_behavior(self) -> None:
        if not self._preset_records:
            self._settings = AppSettings.built_in_defaults()
            self._active_preset_name = None
            self._sync_preset_combo_to_active_state()
            self._log("info", "No user presets found yet. Using built-in defaults.")
            return

        requested_name = resolve_startup_preset_name(self._app_config, set(self._preset_records))
        raw_requested_name = ""
        if self._app_config.startup_mode == "specific_preset":
            raw_requested_name = self._app_config.startup_preset_name
        elif self._app_config.startup_mode == "last_used_preset":
            raw_requested_name = self._app_config.last_used_preset_name

        if requested_name and self.apply_preset_by_name(requested_name, save_last_used=True):
            return

        self._settings = AppSettings.built_in_defaults()
        self._active_preset_name = None
        self._sync_preset_combo_to_active_state()
        if raw_requested_name and requested_name is None:
            self._log("warning", f"Startup preset '{raw_requested_name}' is unavailable. Using built-in defaults.")
        else:
            self._log("info", "Using built-in defaults.")

    def apply_preset_by_name(self, preset_name: str, save_last_used: bool) -> bool:
        record = self._preset_records.get(preset_name)
        if record is None:
            return False
        try:
            loaded = load_preset(record.path)
        except Exception as exc:
            self._log("warning", f"Preset warning: could not load '{preset_name}': {exc}")
            return False
        self._settings = loaded.settings.clone()
        self._active_preset_name = loaded.name
        self._suppress_preset_change = True
        try:
            self._sync_preset_combo_to_active_state()
        finally:
            self._suppress_preset_change = False
        self._log("info", f"Applied preset '{loaded.name}' from {loaded.path.name}")
        if save_last_used and self._app_config.last_used_preset_name != loaded.name:
            self._app_config.last_used_preset_name = loaded.name
            self._save_app_config_with_log()
        return True

    def _toggle_connection(self, checked: bool) -> None:
        if checked:
            if not self._settings.serial.port:
                QMessageBox.warning(self, "Missing serial port", "Set a serial port in Settings before connecting.")
                self.connect_button.setChecked(False)
                return
            self._serial.connect_port(self._settings.serial)
        else:
            self._serial.disconnect_port()
            self._log("info", "Serial connection closed.")

    def _open_settings(self) -> None:
        previous_settings = self._settings.clone()
        previous_config = AppConfig.from_dict(self._app_config.to_dict())
        dialog = SettingsDialog(
            paths=self._paths,
            app_config=self._app_config,
            current_settings=self._settings,
            available_preset_names=list(self._preset_records),
            serial_in_use=self._serial.is_connected(),
            log_callback=self._log,
            parent=self,
        )
        if not dialog.exec():
            return

        new_settings = dialog.selected_settings()
        new_config = dialog.selected_app_config()
        was_connected = self._serial.is_connected()
        reconnect_required = was_connected and serial_settings_require_reconnect(previous_settings.serial, new_settings.serial)

        self._settings = new_settings
        self._app_config = new_config
        self._log_created_dirs(self._paths.ensure_runtime_dirs(self._app_config))
        self._active_preset_name = None

        config_changed = self._app_config.to_dict() != previous_config.to_dict()
        if config_changed:
            self._save_app_config_with_log()

        if (
            self._app_config.log_mode != previous_config.log_mode
            or self._paths.logs_dir(self._app_config) != self._paths.logs_dir(previous_config)
        ):
            self._reset_session_logger()
            self._log("info", f"Logs path: {self._paths.logs_dir(self._app_config)}")
            self._log("info", f"Log recording mode: {self._app_config.log_mode}")

        if self._paths.presets_dir(self._app_config) != self._paths.presets_dir(previous_config):
            self._log("info", f"Presets path: {self._paths.presets_dir(self._app_config)}")

        self.refresh_presets()
        self._sync_preset_combo_to_active_state()

        if reconnect_required:
            self._serial.disconnect_port()
            self._log("info", "Serial connection closed.")
            self._log("info", f"Reconnecting with updated serial settings on {self._settings.serial.port}")
            self._serial.connect_port(self._settings.serial)
        elif config_changed or self._settings.to_dict() != previous_settings.to_dict():
            self._log("info", "Configuration updated.")

    def _show_about_dialog(self) -> None:
        QMessageBox.about(self, f"About {APP_NAME}", about_text())

    def _on_preset_selected(self, preset_name: str) -> None:
        if self._suppress_preset_change or not preset_name:
            return
        if self._serial.is_connected():
            self._serial.disconnect_port()
            self.connect_button.setChecked(False)
            self._log("info", "Serial connection closed.")
        self.apply_preset_by_name(preset_name, save_last_used=True)

    def _on_state_changed(self, state: str) -> None:
        self._set_status(state)
        if state == "connected":
            self.connect_button.setChecked(True)
            self.connect_button.setText("Disconnect")
            self._log("info", f"Connected to {self._settings.serial.port}")
        elif state == "connecting":
            self._log("info", f"Connecting to serial device on {self._settings.serial.port}")
            self.connect_button.setText("Connect")
        elif state == "error":
            self.connect_button.setChecked(False)
            self.connect_button.setText("Connect")
        else:
            self.connect_button.setChecked(False)
            self.connect_button.setText("Connect")
            self._log("info", "Disconnected.")

    def _on_line_received(self, raw_line: str) -> None:
        result = self._parser.process(raw_line, self._settings.parsing)
        if not result.ok:
            if self._app_config.line_log_mode == "verbose":
                self._log("info", f"Raw received line: {raw_line!r}")
                self._log("warning", result.message)
            else:
                self._log("warning", f"Raw: {raw_line!r} → Parse warning: {result.message}")
            return

        send_result = self._sender.send_text_and_action(
            result.processed_text,
            self._settings.output.post_action,
            self._settings.output.custom_sequence,
        )
        if not send_result.ok:
            prefix = "Text injection error" if send_result.stage == "text" else "After-send action error"
            self._log("error", f"{prefix}: {send_result.message} (raw={raw_line!r}, text={result.processed_text!r})")
            return

        if self._app_config.line_log_mode == "verbose":
            self._log("info", f"Raw received line: {raw_line!r}")
            self._log("info", f"Sent value ({result.mode}): {result.processed_text!r}")
        else:
            label = "Parsed" if result.mode == "parsed" else "Sent"
            self._log("info", f"Raw: {raw_line!r} → {label}: {result.processed_text!r}")

    def _save_app_config_with_log(self) -> None:
        try:
            save_app_config(self._paths.config_path, self._app_config)
        except Exception as exc:
            self._log("warning", f"Could not save configuration: {exc}")
            return

    def _auto_connect_if_enabled(self) -> None:
        if not self._app_config.connect_on_startup or self._serial.is_connected():
            return
        if not self._settings.serial.port:
            return
        self._log("info", f"Auto-connecting to {self._settings.serial.port}")
        self._serial.connect_port(self._settings.serial)

    def _set_status(self, state: str) -> None:
        mapping = {
            "connected": ("Connected", "#4ade80"),
            "connecting": ("Connecting", "#f59e0b"),
            "error": ("Error", "#f87171"),
            "disconnected": ("Disconnected", "#94a3b8"),
        }
        label, color = mapping.get(state, mapping["disconnected"])
        self.status_text.setText(label)
        self.status_dot.setStyleSheet(f"color: {color};")

    def _log(self, level: str, message: str) -> None:
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_view.append(f"[{timestamp}] {message}")
        self.log_view.moveCursor(QTextCursor.End)
        if self._session_logger is not None:
            self._session_logger.write(level, message)

    def closeEvent(self, event) -> None:  # noqa: N802
        self._log("info", "User requested close.")
        self._serial.disconnect_port(wait_ms=2000)
        self._log("info", "Serial connection closed.")
        self._log("info", "Application shutdown complete.")
        if self._session_logger is not None:
            self._session_logger.close()
        sys.excepthook = self._original_excepthook
        logging.shutdown()
        super().closeEvent(event)
