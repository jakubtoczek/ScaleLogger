from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from PySide6.QtCore import Qt, QTimer, Signal
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDialog,
    QFileDialog,
    QFormLayout,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QInputDialog,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from ..config import (
    AppConfig,
    AppSettings,
    BAUDRATE_PRESETS,
    DATA_BITS,
    LOG_MODES,
    PARITY_LABELS,
    PARITY_VALUES,
    PARSE_MODES,
    POST_ACTIONS,
    STOP_BITS,
    TIMEOUT_PRESETS,
    normalize_parity_value,
    normalize_serial_port_name,
    save_app_config,
)
from ..key_sequences import CUSTOM_SEQUENCE_ACTION, format_key_sequence, key_token_from_qt_event
from ..paths import AppPaths
from ..presets import discover_presets, save_preset
from ..serial_tools import list_available_serial_ports, parity_label_for_value, port_value_from_label
from ..version import APP_LOGS_DIRNAME, APP_NAME, APP_PRESETS_DIRNAME
from .test_receive_dialog import TestReceiveDialog

LOG_MODE_LABELS = {
    "none": "No log",
    "single_file": "Single file",
    "per_session": "New file per session",
}
STARTUP_PLACEHOLDER = "Last used / defaults"


class KeyCaptureButton(QPushButton):
    keyCaptured = Signal(str)

    def __init__(self, parent=None) -> None:
        super().__init__("Capture Key", parent)
        self._capturing = False
        self.clicked.connect(self._arm_capture)
        self.setToolTip("Click, then press a key to append it to the custom after-send sequence.")

    def _arm_capture(self) -> None:
        self._capturing = True
        self.setText("Press key…")
        self.setFocus(Qt.FocusReason.MouseFocusReason)

    def keyPressEvent(self, event) -> None:  # noqa: N802
        if not self._capturing:
            super().keyPressEvent(event)
            return
        token = key_token_from_qt_event(event.key(), event.text())
        self._capturing = False
        self.setText("Capture Key")
        if token is not None:
            self.keyCaptured.emit(token)
            event.accept()
            return
        super().keyPressEvent(event)


class SettingsDialog(QDialog):
    def __init__(
        self,
        paths: AppPaths,
        app_config: AppConfig,
        current_settings: AppSettings,
        available_preset_names: list[str],
        serial_in_use: bool = False,
        log_callback: Callable[[str, str], None] | None = None,
        parent=None,
    ) -> None:
        super().__init__(parent)
        self.setWindowTitle(f"{APP_NAME} Settings")
        self.resize(560, 560)
        self._paths = paths
        self._app_config = AppConfig.from_dict(app_config.to_dict())
        self._settings = current_settings.clone()
        self._available_preset_names = list(available_preset_names)
        self._serial_in_use = serial_in_use
        self._log_callback = log_callback
        self._custom_sequence = list(self._settings.output.custom_sequence)
        self._feedback_timer = QTimer(self)
        self._feedback_timer.setSingleShot(True)
        self._feedback_timer.timeout.connect(self._restore_scan_button_text)
        self._build_ui()
        self._load_to_widgets()
        self._scan_ports(initial=True)

    def selected_settings(self) -> AppSettings:
        return self._settings

    def selected_app_config(self) -> AppConfig:
        return self._app_config

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)
        tabs = QTabWidget()
        tabs.addTab(self._build_serial_tab(), "Serial")
        tabs.addTab(self._build_output_tab(), "Output")
        tabs.addTab(self._build_application_tab(), "Application")
        layout.addWidget(tabs)

        action_row = QHBoxLayout()
        self.save_config_button = QPushButton("Save Configuration")
        self.save_config_button.clicked.connect(self._save_app_config_now)
        self.save_preset_button = QPushButton("Save as Preset")
        self.save_preset_button.clicked.connect(self._save_current_as_preset)
        action_row.addWidget(self.save_config_button)
        action_row.addWidget(self.save_preset_button)
        action_row.addStretch(1)
        layout.addLayout(action_row)

        buttons = QHBoxLayout()
        buttons.addStretch(1)
        apply_button = QPushButton("Apply")
        apply_button.clicked.connect(self._accept)
        cancel_button = QPushButton("Cancel")
        cancel_button.clicked.connect(self.reject)
        buttons.addWidget(apply_button)
        buttons.addWidget(cancel_button)
        layout.addLayout(buttons)

    def _build_serial_tab(self) -> QWidget:
        tab = QWidget()
        layout = QVBoxLayout(tab)
        form = QFormLayout()

        self.port_combo = QComboBox()
        self.port_combo.setEditable(True)
        self.port_combo.setInsertPolicy(QComboBox.NoInsert)
        self.port_combo.setToolTip("Scan ports or type a port name manually. Windows values such as 6 are normalized to COM6.")

        self.scan_ports_button = QPushButton("Scan Ports")
        self.scan_ports_button.clicked.connect(self._scan_ports)
        self.test_receive_button = QPushButton("Test Receive")
        self.test_receive_button.clicked.connect(self._open_test_receive)

        port_row = QHBoxLayout()
        port_row.addWidget(self.port_combo, 1)
        port_row.addWidget(self.scan_ports_button)
        port_row.addWidget(self.test_receive_button)
        form.addRow("Port", self._wrap_layout(port_row))

        self.port_feedback_label = QLabel()
        self.port_feedback_label.setWordWrap(True)
        self.port_feedback_label.setToolTip("Short feedback about the last port scan.")
        form.addRow("", self.port_feedback_label)

        self.baud_combo = self._editable_combo(BAUDRATE_PRESETS, "Common baud rates. You can still type a custom value.")
        self.databits_combo = QComboBox()
        self.databits_combo.addItems([str(value) for value in DATA_BITS])
        self.parity_combo = self._editable_combo(
            tuple(PARITY_LABELS[value] for value in PARITY_VALUES),
            "Pick or type N/None, O/Odd, E/Even, M/Mark, or S/Space.",
        )
        self.stopbits_combo = QComboBox()
        self.stopbits_combo.addItems([str(value).rstrip("0").rstrip(".") for value in STOP_BITS])
        self.timeout_combo = self._editable_combo(TIMEOUT_PRESETS, "Seconds. Common values are suggested, but you can type your own.")
        self.eol_combo = QComboBox()
        self.eol_combo.addItems([r"\r\n", r"\n", r"\r"])

        form.addRow("Baud", self.baud_combo)
        form.addRow("Data bits", self.databits_combo)
        form.addRow("Parity", self.parity_combo)
        form.addRow("Stop bits", self.stopbits_combo)
        form.addRow("Timeout", self.timeout_combo)
        form.addRow("Line ending", self.eol_combo)
        layout.addLayout(form)
        layout.addStretch(1)
        return tab

    def _build_output_tab(self) -> QWidget:
        tab = QWidget()
        layout = QVBoxLayout(tab)

        brief_note = QLabel("Send value, then after-send action.")
        brief_note.setToolTip(f"{APP_NAME} sends text to the currently focused window first, then sends the selected after-send action.")
        layout.addWidget(brief_note)

        formatting_group = QGroupBox("Value formatting")
        formatting_form = QFormLayout(formatting_group)
        self.mode_combo = QComboBox()
        self.mode_combo.addItems(PARSE_MODES)
        self.trim_checkbox = QCheckBox("Trim whitespace")
        self.strip_suffix_checkbox = QCheckBox("Strip suffix")
        self.suffix_edit = QLineEdit()
        self.suffix_edit.setPlaceholderText("g")
        self.normalize_sign_checkbox = QCheckBox("Normalize sign spacing")
        self.normalize_sign_checkbox.setToolTip('Example: "-  0.12" becomes "-0.12".')
        self.preserve_plus_checkbox = QCheckBox("Preserve leading +")
        self.numeric_validation_checkbox = QCheckBox("Require numeric result")
        formatting_form.addRow("Mode", self.mode_combo)
        formatting_form.addRow(self.trim_checkbox)
        formatting_form.addRow(self.strip_suffix_checkbox)
        formatting_form.addRow("Known suffix", self.suffix_edit)
        formatting_form.addRow(self.normalize_sign_checkbox)
        formatting_form.addRow(self.preserve_plus_checkbox)
        formatting_form.addRow(self.numeric_validation_checkbox)

        after_send_group = QGroupBox("After-send")
        after_send_layout = QVBoxLayout(after_send_group)
        after_send_form = QFormLayout()
        self.post_action_combo = QComboBox()
        self.post_action_combo.addItems(POST_ACTIONS)
        self.post_action_combo.currentTextChanged.connect(self._update_custom_sequence_visibility)
        after_send_form.addRow("Action", self.post_action_combo)
        after_send_layout.addLayout(after_send_form)

        self.custom_sequence_widget = QWidget()
        custom_layout = QVBoxLayout(self.custom_sequence_widget)
        self.custom_sequence_display = QLineEdit()
        self.custom_sequence_display.setReadOnly(True)
        self.custom_sequence_display.setToolTip("Ordered keys that will be sent after the value.")
        buttons = QHBoxLayout()
        self.capture_key_button = KeyCaptureButton()
        self.capture_key_button.keyCaptured.connect(self._append_custom_sequence_key)
        self.remove_key_button = QPushButton("Remove Last")
        self.remove_key_button.clicked.connect(self._remove_last_custom_key)
        self.clear_sequence_button = QPushButton("Clear")
        self.clear_sequence_button.clicked.connect(self._clear_custom_sequence)
        buttons.addWidget(self.capture_key_button)
        buttons.addWidget(self.remove_key_button)
        buttons.addWidget(self.clear_sequence_button)
        custom_layout.addWidget(self.custom_sequence_display)
        custom_layout.addLayout(buttons)
        after_send_layout.addWidget(self.custom_sequence_widget)

        layout.addWidget(formatting_group)
        layout.addWidget(after_send_group)
        layout.addStretch(1)
        return tab

    def _build_application_tab(self) -> QWidget:
        tab = QWidget()
        layout = QVBoxLayout(tab)

        app_group = QGroupBox("Configuration")
        grid = QGridLayout(app_group)
        self.presets_folder_edit = QLineEdit()
        self.logs_folder_edit = QLineEdit()
        presets_browse = QPushButton("Browse")
        presets_browse.clicked.connect(self._browse_presets_folder)
        logs_browse = QPushButton("Browse")
        logs_browse.clicked.connect(self._browse_logs_folder)
        self.log_mode_combo = QComboBox()
        for value in LOG_MODES:
            self.log_mode_combo.addItem(LOG_MODE_LABELS[value], value)
        self.connect_on_startup_checkbox = QCheckBox("Connect on startup")
        self.connect_on_startup_checkbox.setToolTip("Connect automatically after startup using the active serial settings.")
        self.startup_preset_combo = QComboBox()
        self.startup_preset_combo.setToolTip("Leave empty to reuse the last used preset. If none exists, built-in defaults are used.")

        grid.addWidget(QLabel("Presets folder"), 0, 0)
        grid.addWidget(self.presets_folder_edit, 0, 1)
        grid.addWidget(presets_browse, 0, 2)
        grid.addWidget(QLabel("Logs folder"), 1, 0)
        grid.addWidget(self.logs_folder_edit, 1, 1)
        grid.addWidget(logs_browse, 1, 2)
        grid.addWidget(QLabel("Log mode"), 2, 0)
        grid.addWidget(self.log_mode_combo, 2, 1, 1, 2)
        grid.addWidget(self.connect_on_startup_checkbox, 3, 0, 1, 3)
        grid.addWidget(QLabel("Startup preset"), 4, 0)
        grid.addWidget(self.startup_preset_combo, 4, 1, 1, 2)

        self.path_note = QLabel()
        self.path_note.setTextInteractionFlags(Qt.TextSelectableByMouse)
        self.path_note.setWordWrap(True)

        layout.addWidget(app_group)
        layout.addWidget(self.path_note)
        layout.addStretch(1)
        return tab

    def _editable_combo(self, values: tuple[str, ...], tooltip: str) -> QComboBox:
        combo = QComboBox()
        combo.setEditable(True)
        combo.addItems(list(values))
        combo.setToolTip(tooltip)
        return combo

    def _wrap_layout(self, layout) -> QWidget:
        widget = QWidget()
        widget.setLayout(layout)
        return widget

    def _scan_ports(self, initial: bool = False) -> None:
        current_port = self._current_port_value() or self._settings.serial.port
        self.scan_ports_button.setText("Scanning…")
        self._port_entries = list_available_serial_ports()
        self.port_combo.clear()
        for entry in self._port_entries:
            self.port_combo.addItem(entry.label, entry.port)
        self.port_combo.setEditText(current_port)
        matched_index = next((index for index, entry in enumerate(self._port_entries) if entry.port == current_port), -1)
        if matched_index >= 0:
            self.port_combo.setCurrentIndex(matched_index)

        count = len(self._port_entries)
        if count == 0:
            feedback = "No ports detected"
        elif count == 1:
            feedback = "Detected 1 port"
        else:
            feedback = f"Detected {count} ports"
        self.port_feedback_label.setText(feedback)
        self.port_combo.setToolTip("Detected ports updated. You can also type a port manually.")
        if not initial:
            self._log_callback and self._log_callback("info", f"Detected {count} serial ports in Settings.")
        self.scan_ports_button.setText("Updated")
        self._feedback_timer.start(1200)

    def _restore_scan_button_text(self) -> None:
        self.scan_ports_button.setText("Scan Ports")

    def _refresh_startup_preset_choices(self, select_name: str = "") -> None:
        folder = self._paths.resolve_user_path(self.presets_folder_edit.text().strip(), APP_PRESETS_DIRNAME)
        discovery = discover_presets(folder)
        self._available_preset_names = [record.name for record in discovery.records]
        self.startup_preset_combo.clear()
        self.startup_preset_combo.addItem(STARTUP_PLACEHOLDER, "")
        for name in self._available_preset_names:
            self.startup_preset_combo.addItem(name, name)
        if select_name and select_name in self._available_preset_names:
            self.startup_preset_combo.setCurrentText(select_name)
        else:
            self.startup_preset_combo.setCurrentIndex(0)
        self._update_path_note("\n".join(discovery.warnings))

    def _load_to_widgets(self) -> None:
        settings = self._settings
        config = self._app_config
        self.baud_combo.setEditText(str(settings.serial.baudrate))
        self.databits_combo.setCurrentText(str(settings.serial.databits))
        self.parity_combo.setEditText(parity_label_for_value(settings.serial.parity))
        self.stopbits_combo.setCurrentText(str(settings.serial.stopbits).rstrip("0").rstrip("."))
        self.timeout_combo.setEditText(f"{settings.serial.timeout:.2f}")
        self.eol_combo.setCurrentText(settings.serial.eol.encode("unicode_escape").decode("ascii"))

        self.mode_combo.setCurrentText(settings.parsing.mode)
        self.trim_checkbox.setChecked(settings.parsing.trim_whitespace)
        self.strip_suffix_checkbox.setChecked(settings.parsing.strip_suffix)
        self.suffix_edit.setText(settings.parsing.suffix)
        self.normalize_sign_checkbox.setChecked(settings.parsing.normalize_sign)
        self.preserve_plus_checkbox.setChecked(not settings.parsing.drop_plus_sign)
        self.numeric_validation_checkbox.setChecked(settings.parsing.numeric_validation)
        self.post_action_combo.setCurrentText(settings.output.post_action)
        self._update_custom_sequence_display()
        self._update_custom_sequence_visibility(settings.output.post_action)

        self.presets_folder_edit.setText(config.presets_folder)
        self.logs_folder_edit.setText(config.logs_folder)
        self.log_mode_combo.setCurrentIndex(LOG_MODES.index(config.log_mode))
        self.connect_on_startup_checkbox.setChecked(config.connect_on_startup)
        selected_startup = config.startup_preset_name if config.startup_mode == "specific_preset" else ""
        self._refresh_startup_preset_choices(selected_startup)
        self._update_path_note()

    def _current_port_value(self) -> str:
        text_value = port_value_from_label(self.port_combo.currentText())
        if text_value:
            return text_value
        data = self.port_combo.currentData()
        return data.strip() if isinstance(data, str) else ""

    def _read_baudrate(self) -> int:
        return int(self.baud_combo.currentText().strip())

    def _read_timeout(self) -> float:
        return float(self.timeout_combo.currentText().strip().replace(",", "."))

    def _read_widgets(self) -> tuple[AppSettings, AppConfig]:
        settings = AppSettings.built_in_defaults()
        settings.serial.port = normalize_serial_port_name(self._current_port_value(), system_name="Windows")
        settings.serial.baudrate = self._read_baudrate()
        settings.serial.databits = int(self.databits_combo.currentText())
        settings.serial.parity = normalize_parity_value(self.parity_combo.currentText(), settings.serial.parity)
        settings.serial.stopbits = float(self.stopbits_combo.currentText())
        settings.serial.timeout = self._read_timeout()
        settings.serial.eol = self.eol_combo.currentText().encode("utf-8").decode("unicode_escape")

        settings.parsing.mode = self.mode_combo.currentText()
        settings.parsing.trim_whitespace = self.trim_checkbox.isChecked()
        settings.parsing.strip_suffix = self.strip_suffix_checkbox.isChecked()
        settings.parsing.suffix = self.suffix_edit.text()
        settings.parsing.normalize_sign = self.normalize_sign_checkbox.isChecked()
        settings.parsing.drop_plus_sign = not self.preserve_plus_checkbox.isChecked()
        settings.parsing.numeric_validation = self.numeric_validation_checkbox.isChecked()
        settings.output.post_action = self.post_action_combo.currentText()
        settings.output.custom_sequence = list(self._custom_sequence)
        settings.validate()

        startup_preset_name = self.startup_preset_combo.currentData() or ""
        config = AppConfig(
            presets_folder=self._folder_value_for_config(self.presets_folder_edit.text(), APP_PRESETS_DIRNAME),
            logs_folder=self._folder_value_for_config(self.logs_folder_edit.text(), APP_LOGS_DIRNAME),
            log_mode=self.log_mode_combo.currentData(),
            line_log_mode=self._app_config.line_log_mode,
            connect_on_startup=self.connect_on_startup_checkbox.isChecked(),
            startup_mode="specific_preset" if startup_preset_name else "last_used_preset",
            startup_preset_name=startup_preset_name,
            last_used_preset_name=self._app_config.last_used_preset_name,
        )
        config.validate()
        return settings, config

    def _update_custom_sequence_visibility(self, action: str) -> None:
        is_custom = action == CUSTOM_SEQUENCE_ACTION
        self.custom_sequence_widget.setVisible(is_custom)
        self.custom_sequence_display.setVisible(is_custom)

    def _append_custom_sequence_key(self, token: str) -> None:
        self._custom_sequence.append(token)
        self._update_custom_sequence_display()

    def _remove_last_custom_key(self) -> None:
        if self._custom_sequence:
            self._custom_sequence.pop()
            self._update_custom_sequence_display()

    def _clear_custom_sequence(self) -> None:
        self._custom_sequence.clear()
        self._update_custom_sequence_display()

    def _update_custom_sequence_display(self) -> None:
        self.custom_sequence_display.setText(format_key_sequence(self._custom_sequence))

    def _accept(self) -> None:
        try:
            self._settings, self._app_config = self._read_widgets()
        except ValueError as exc:
            QMessageBox.warning(self, "Invalid settings", str(exc))
            return
        self.accept()

    def _save_app_config_now(self) -> None:
        try:
            _, config = self._read_widgets()
            save_app_config(self._paths.config_path, config)
        except Exception as exc:
            QMessageBox.warning(self, "Save failed", f"Could not save configuration:\n{exc}")
            return
        self._app_config = config
        QMessageBox.information(self, "Saved", f"Saved configuration to:\n{self._paths.config_path}")

    def _save_current_as_preset(self) -> None:
        try:
            settings, config = self._read_widgets()
        except ValueError as exc:
            QMessageBox.warning(self, "Invalid settings", str(exc))
            return

        preset_name, ok = QInputDialog.getText(self, "Preset name", "Preset name")
        if not ok or not preset_name.strip():
            return
        safe_name = preset_name.strip()
        default_path = self._paths.presets_dir(config) / f"{safe_name}.json"
        path_str, _ = QFileDialog.getSaveFileName(self, "Save preset", str(default_path), "JSON preset (*.json)")
        if not path_str:
            return

        path = Path(path_str)
        if path.suffix.lower() != ".json":
            path = path.with_suffix(".json")

        try:
            save_preset(path, safe_name, settings)
        except Exception as exc:
            QMessageBox.warning(self, "Preset save failed", f"Could not save preset:\n{exc}")
            return

        QMessageBox.information(self, "Preset saved", f"Saved preset to:\n{path}")
        self._refresh_startup_preset_choices(select_name=safe_name)

    def _browse_presets_folder(self) -> None:
        folder = QFileDialog.getExistingDirectory(
            self,
            "Select presets folder",
            str(self._paths.resolve_user_path(self.presets_folder_edit.text().strip(), APP_PRESETS_DIRNAME)),
        )
        if folder:
            self.presets_folder_edit.setText(self._folder_value_for_config(folder, APP_PRESETS_DIRNAME))
            self._refresh_startup_preset_choices()

    def _browse_logs_folder(self) -> None:
        folder = QFileDialog.getExistingDirectory(
            self,
            "Select logs folder",
            str(self._paths.resolve_user_path(self.logs_folder_edit.text().strip(), APP_LOGS_DIRNAME)),
        )
        if folder:
            self.logs_folder_edit.setText(self._folder_value_for_config(folder, APP_LOGS_DIRNAME))
            self._update_path_note()

    def _open_test_receive(self) -> None:
        try:
            settings, _ = self._read_widgets()
        except ValueError as exc:
            QMessageBox.warning(self, "Invalid serial settings", str(exc))
            return
        dialog = TestReceiveDialog(settings.serial, self._serial_in_use, self._log_callback, parent=self)
        dialog.exec()

    def _update_path_note(self, extra_warning: str = "") -> None:
        presets_dir = self._paths.resolve_user_path(self.presets_folder_edit.text().strip(), APP_PRESETS_DIRNAME)
        logs_dir = self._paths.resolve_user_path(self.logs_folder_edit.text().strip(), APP_LOGS_DIRNAME)
        lines = [
            f"Data: {self._paths.data_dir}",
            f"Config: {self._paths.config_path}",
            f"Presets: {presets_dir}",
            f"Logs: {logs_dir}",
        ]
        if extra_warning:
            lines.append(extra_warning)
        self.path_note.setText("\n".join(lines))

    def _folder_value_for_config(self, raw_value: str, default_name: str) -> str:
        resolved = self._paths.resolve_user_path(raw_value.strip(), default_name)
        try:
            relative = resolved.relative_to(self._paths.data_dir)
        except ValueError:
            return default_name
        return str(relative) or default_name
