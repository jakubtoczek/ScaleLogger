from __future__ import annotations

import ctypes
import json
import tempfile
from pathlib import Path
import unittest
from unittest import mock

from app.config import (
    AppConfig,
    AppSettings,
    load_app_config,
    normalize_parity_value,
    normalize_serial_port_name,
    save_app_config,
    serial_settings_require_reconnect,
)
from app.key_sequences import CUSTOM_SEQUENCE_ACTION, format_key_sequence, normalize_key_token
from app.output_sender import KEYEVENTF_KEYUP, KEYEVENTF_UNICODE, WindowsInputSender
from app.parser import ScaleLineParser
from app.paths import AppPaths
from app.presets import discover_presets, load_preset, resolve_startup_preset_name, save_preset
from app.release_support import build_manifest_text, cleanup_nuitka_artifacts, verify_release_executable
from app.serial_manager import discard_open_stale_input
from app.serial_tools import list_available_serial_ports, port_value_from_label
from app.version import APP_CONFIG_FILENAME, APP_LOGS_DIRNAME, APP_NAME, APP_PRESETS_DIRNAME, about_text


class ConfigAndPresetTests(unittest.TestCase):
    def test_config_round_trip(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "ScaleLogger.config.json"
            config = AppConfig(
                presets_folder="custom-presets",
                logs_folder="custom-logs",
                log_mode="single_file",
                line_log_mode="verbose",
                connect_on_startup=False,
                startup_mode="specific_preset",
                startup_preset_name="TR-602",
                last_used_preset_name="TR-602",
            )
            save_app_config(config_path, config)
            loaded = load_app_config(config_path)
            self.assertEqual(loaded.startup_mode, "specific_preset")
            self.assertEqual(loaded.presets_folder, "custom-presets")
            self.assertEqual(loaded.log_mode, "single_file")
            self.assertEqual(loaded.line_log_mode, "verbose")
            self.assertFalse(loaded.connect_on_startup)

    def test_built_in_defaults_use_expected_serial_and_output_defaults(self) -> None:
        settings = AppSettings.built_in_defaults()
        self.assertEqual(settings.serial.port, "COM6")
        self.assertEqual(settings.serial.eol, "\r\n")
        self.assertEqual(settings.serial.timeout, 1.0)
        self.assertEqual(settings.output.post_action, "down")

    def test_preset_discovery_skips_invalid_json(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            valid_path = folder / "TR-602.json"
            invalid_path = folder / "broken.json"
            save_preset(valid_path, "TR-602", AppSettings.built_in_defaults())
            invalid_path.write_text("{not-json", encoding="utf-8")

            discovery = discover_presets(folder)
            self.assertEqual([record.name for record in discovery.records], ["TR-602"])
            self.assertEqual(len(discovery.warnings), 1)

    def test_preset_discovery_skips_duplicate_preset_names(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            settings = AppSettings.built_in_defaults()
            save_preset(folder / "a.json", "TR-602", settings)
            save_preset(folder / "b.json", "TR-602", settings)

            discovery = discover_presets(folder)
            self.assertEqual([record.name for record in discovery.records], ["TR-602"])
            self.assertEqual(len(discovery.warnings), 1)
            self.assertIn("duplicate preset name 'TR-602'", discovery.warnings[0])

    def test_legacy_nested_preset_format_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "legacy.json"
            path.write_text('{"serial": {"port": "COM1"}}', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "legacy nested preset format is not supported"):
                load_preset(path)

    def test_paths_resolve_relative_directories(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            data_dir = Path(tmp) / APP_NAME
            paths = AppPaths(
                data_dir=data_dir,
                bundle_dir=data_dir / "bundle",
                executable_dir=data_dir / "bin",
                config_path=data_dir / APP_CONFIG_FILENAME,
            )
        config = AppConfig(presets_folder="presets", logs_folder="logs")
        self.assertTrue(str(paths.presets_dir(config)).endswith("presets"))
        self.assertTrue(str(paths.logs_dir(config)).endswith("logs"))

    def test_paths_expand_percent_style_environment_variables(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            data_dir = Path(tmp) / APP_NAME
            paths = AppPaths(
                data_dir=data_dir,
                bundle_dir=data_dir / "bundle",
                executable_dir=data_dir / "bin",
                config_path=data_dir / APP_CONFIG_FILENAME,
            )
            with mock.patch.dict("os.environ", {"USERPROFILE": str(data_dir)}):
                resolved = paths.resolve_user_path(r"%USERPROFILE%\\ScaleLogger\\logs", APP_LOGS_DIRNAME)
            self.assertEqual(resolved, data_dir / "ScaleLogger" / "logs")

    def test_paths_keep_user_data_inside_persistent_data_dir(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            data_dir = Path(tmp) / APP_NAME
            paths = AppPaths(
                data_dir=data_dir,
                bundle_dir=data_dir / "bundle",
                executable_dir=data_dir / "bin",
                config_path=data_dir / APP_CONFIG_FILENAME,
            )
            created = paths.ensure_default_storage()
            self.assertIn(str(data_dir), created)
            self.assertTrue((data_dir / APP_PRESETS_DIRNAME).exists())
            self.assertTrue((data_dir / APP_LOGS_DIRNAME).exists())
            self.assertEqual(paths.resolve_user_path("../temp", APP_PRESETS_DIRNAME), data_dir / "temp")
            self.assertEqual(paths.resolve_user_path("/tmp/outside", APP_LOGS_DIRNAME), data_dir / "outside")

    def test_discover_uses_user_home_without_hardcoded_windows_path(self) -> None:
        fake_home = Path("/tmp/Utilisateur Localise")
        with mock.patch("app.paths.Path.home", return_value=fake_home):
            paths = AppPaths.discover()
        self.assertEqual(paths.data_dir, fake_home / APP_NAME)

    def test_example_config_paths_remain_relative(self) -> None:
        payload = json.loads(Path("ScaleLogger.config.json.example").read_text(encoding="utf-8"))
        self.assertEqual(payload["logs_folder"], "logs")
        self.assertEqual(payload["presets_folder"], "presets")

    def test_parity_normalization_accepts_short_and_long_labels(self) -> None:
        self.assertEqual(normalize_parity_value("O"), "O")
        self.assertEqual(normalize_parity_value("Odd"), "O")
        self.assertEqual(normalize_parity_value("even"), "E")
        self.assertEqual(normalize_parity_value("None"), "N")
        self.assertEqual(normalize_parity_value("Mark"), "M")
        self.assertEqual(normalize_parity_value("space"), "S")

    def test_windows_port_normalization_accepts_numeric_and_com_prefix_forms(self) -> None:
        self.assertEqual(normalize_serial_port_name("6", system_name="Windows"), "COM6")
        self.assertEqual(normalize_serial_port_name("COM6", system_name="Windows"), "COM6")
        self.assertEqual(normalize_serial_port_name("com06", system_name="Windows"), "COM6")
        self.assertEqual(normalize_serial_port_name("ttyUSB0", system_name="Windows"), "ttyUSB0")

    def test_port_value_from_label_preserves_manual_and_detected_values(self) -> None:
        self.assertEqual(port_value_from_label("COM6 — USB Serial Device"), "COM6")
        self.assertEqual(port_value_from_label("6"), "COM6")
        self.assertEqual(port_value_from_label("ttyUSB0"), "ttyUSB0")

    def test_serial_port_listing_uses_sorted_labels(self) -> None:
        fake_ports = [
            mock.Mock(device="COM10", description="USB Serial", manufacturer="FTDI", product="Adapter"),
            mock.Mock(device="COM2", description="Lab Balance", manufacturer="", product=""),
        ]
        with mock.patch("app.serial_tools.list_ports.comports", return_value=fake_ports):
            entries = list_available_serial_ports()
        self.assertEqual([entry.port for entry in entries], ["COM2", "COM10"])
        self.assertIn("Lab Balance", entries[0].label)
        self.assertIn("USB Serial", entries[1].label)

    def test_startup_preset_resolution_is_honest_about_fallback(self) -> None:
        self.assertIsNone(resolve_startup_preset_name(AppConfig(startup_mode="built_in_defaults"), {"TR-602"}))
        missing = AppConfig(startup_mode="specific_preset", startup_preset_name="Missing")
        self.assertIsNone(resolve_startup_preset_name(missing, {"TR-602"}))
        valid = AppConfig(startup_mode="specific_preset", startup_preset_name="TR-602")
        self.assertEqual(resolve_startup_preset_name(valid, {"TR-602"}), "TR-602")

    def test_default_startup_behavior_prefers_last_used_then_built_in_defaults(self) -> None:
        config = AppConfig.built_in_defaults()
        self.assertEqual(config.startup_mode, "last_used_preset")
        self.assertTrue(config.connect_on_startup)
        self.assertEqual(config.line_log_mode, "compact")
        self.assertIsNone(resolve_startup_preset_name(config, {"TR-602"}))
        config.last_used_preset_name = "TR-602"
        self.assertEqual(resolve_startup_preset_name(config, {"TR-602"}), "TR-602")

    def test_serial_reconnect_helper_only_flags_connection_critical_changes(self) -> None:
        current = AppSettings.built_in_defaults().serial
        updated = AppSettings.built_in_defaults().serial
        self.assertFalse(serial_settings_require_reconnect(current, updated))
        updated = AppSettings.built_in_defaults().serial
        updated.timeout = 2.0
        self.assertTrue(serial_settings_require_reconnect(current, updated))

    def test_release_manifest_text_matches_v095_requirements(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            checksum_path = Path(tmp) / "SHA256SUMS.txt"
            checksum_path.write_text("abc123  ScaleLogger.exe\n", encoding="utf-8")
            manifest = build_manifest_text(checksum_path, "ScaleLogger.exe", "ScaleLogger_build_release.bat")
        self.assertIn("Version: 0.95", manifest)
        self.assertIn("Output filename: ScaleLogger.exe", manifest)
        self.assertIn("SHA256 file: SHA256SUMS.txt", manifest)
        self.assertIn("SHA256: abc123", manifest)
        self.assertNotIn("Git commit:", manifest)

    def test_release_cleanup_removes_expected_nuitka_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for dirname in ("main.build", "main.dist", "main.onefile-build"):
                path = root / dirname
                path.mkdir()
                (path / "marker.txt").write_text("x", encoding="utf-8")
            removed, failed = cleanup_nuitka_artifacts(root)
        self.assertEqual(sorted(removed), ["main.build", "main.dist", "main.onefile-build"])
        self.assertEqual(failed, [])

    def test_discard_open_stale_input_resets_and_drains_before_capture(self) -> None:
        port = mock.Mock()
        type(port).in_waiting = mock.PropertyMock(side_effect=[4, 3, 0])
        port.read_all.side_effect = [b"stale", b"old"]
        with mock.patch("app.serial_manager.time.sleep") as sleep_mock:
            discarded = discard_open_stale_input(port, settle_seconds=0.05)
        self.assertEqual(discarded, 8)
        self.assertEqual(port.reset_input_buffer.call_count, 2)
        self.assertEqual(port.read_all.call_count, 2)
        sleep_mock.assert_called()

    def test_release_verifier_rejects_small_stub_executable(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            exe_path = Path(tmp) / "ScaleLogger.exe"
            exe_path.write_bytes(b"stub")
            ok, message = verify_release_executable(exe_path, min_size_bytes=10)
        self.assertFalse(ok)
        self.assertIn("unexpectedly small", message)

    def test_custom_sequence_round_trip_and_display(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "custom-sequence.json"
            settings = AppSettings.built_in_defaults()
            settings.output.post_action = CUSTOM_SEQUENCE_ACTION
            settings.output.custom_sequence = ["down", "tab", "f2"]
            save_preset(path, "Custom", settings)
            loaded = load_preset(path)
        self.assertEqual(loaded.settings.output.post_action, CUSTOM_SEQUENCE_ACTION)
        self.assertEqual(loaded.settings.output.custom_sequence, ["down", "tab", "f2"])
        self.assertEqual(format_key_sequence(loaded.settings.output.custom_sequence), "Down -> Tab -> F2")

    def test_key_token_normalization_accepts_common_aliases(self) -> None:
        self.assertEqual(normalize_key_token("Page Up"), "pageup")
        self.assertEqual(normalize_key_token("Del"), "delete")
        self.assertEqual(normalize_key_token("Escape"), "esc")

    def test_parser_accepts_valid_signed_and_unsigned_values(self) -> None:
        parser = ScaleLineParser()
        settings = AppSettings.built_in_defaults().parsing
        cases = {
            "-  0.00123 g": "-0.00123",
            "+\t 0.00123 g": "+0.00123",
            "0.00123 g": "0.00123",
            "−\t0.500 g": "-0.500",
            "-0": "-0",
            "-0.000": "-0.000",
            "+0": "+0",
            "+0.000": "+0.000",
            "0": "0",
            "0.000": "0.000",
        }
        for raw, expected in cases.items():
            with self.subTest(raw=raw):
                result = parser.process(raw, settings)
                self.assertTrue(result.ok)
                self.assertEqual(result.processed_text, expected)

    def test_parser_can_drop_plus_sign_when_enabled(self) -> None:
        parser = ScaleLineParser()
        settings = AppSettings.built_in_defaults().parsing
        settings.drop_plus_sign = True
        for raw in ["+  0.00123 g", "+0", "+0.000"]:
            with self.subTest(raw=raw):
                result = parser.process(raw, settings)
                self.assertTrue(result.ok)
        self.assertEqual(parser.process("+  0.00123 g", settings).processed_text, "0.00123")
        self.assertEqual(parser.process("+0", settings).processed_text, "0")
        self.assertEqual(parser.process("+0.000", settings).processed_text, "0.000")

    def test_parser_can_drop_minus_sign_when_configured(self) -> None:
        parser = ScaleLineParser()
        settings = AppSettings.built_in_defaults().parsing
        settings.drop_minus_sign = True
        for raw in ["-  0.00123 g", "-0", "-0.000"]:
            with self.subTest(raw=raw):
                result = parser.process(raw, settings)
                self.assertTrue(result.ok)
        self.assertEqual(parser.process("-  0.00123 g", settings).processed_text, "0.00123")
        self.assertEqual(parser.process("-0", settings).processed_text, "0")
        self.assertEqual(parser.process("-0.000", settings).processed_text, "0.000")

    def test_parser_rejects_extra_leading_signs_even_when_plus_drop_is_enabled(self) -> None:
        parser = ScaleLineParser()
        settings = AppSettings.built_in_defaults().parsing
        settings.drop_plus_sign = True
        for raw in ["++0.1 g", "+-0.1 g", "+ +0.1 g"]:
            with self.subTest(raw=raw):
                result = parser.process(raw, settings)
                self.assertFalse(result.ok)

    def test_parser_rejects_malformed_sign_combinations(self) -> None:
        parser = ScaleLineParser()
        settings = AppSettings.built_in_defaults().parsing
        for raw in ["--0.1 g", "+-0.1 g", "-+0.1 g", "+ - 0.1 g", "- + 0.1 g"]:
            with self.subTest(raw=raw):
                result = parser.process(raw, settings)
                self.assertFalse(result.ok)

    def test_parser_rejects_junk_and_trailing_text(self) -> None:
        parser = ScaleLineParser()
        settings = AppSettings.built_in_defaults().parsing
        for raw in ["abc -0.123 g", "-0.123 g ok", "gross -0.123 g", "ERR", "stable", "- 0.00123 gg"]:
            with self.subTest(raw=raw):
                result = parser.process(raw, settings)
                self.assertFalse(result.ok)

    def test_parser_auto_suffix_stripping_handles_blank_suffix_strictly(self) -> None:
        parser = ScaleLineParser()
        settings = AppSettings.built_in_defaults().parsing
        settings.suffix = ""
        valid_cases = {
            "0.123 g": "0.123",
            "- 0.123 g": "-0.123",
            "+ 0.123 g": "+0.123",
        }
        for raw, expected in valid_cases.items():
            with self.subTest(raw=raw):
                result = parser.process(raw, settings)
                self.assertTrue(result.ok)
                self.assertEqual(result.processed_text, expected)

        for raw in ["- 0.123 gg", "-0.123 g ok", "stable g"]:
            with self.subTest(raw=raw):
                result = parser.process(raw, settings)
                self.assertFalse(result.ok)

    def test_parser_explicit_suffix_requires_exact_match(self) -> None:
        parser = ScaleLineParser()
        settings = AppSettings.built_in_defaults().parsing
        settings.suffix = "g"
        self.assertTrue(parser.process("- 0.123 g", settings).ok)
        self.assertFalse(parser.process("- 0.123 gg", settings).ok)

    def test_parser_rejects_decimal_comma_values(self) -> None:
        parser = ScaleLineParser()
        settings = AppSettings.built_in_defaults().parsing
        for raw in ["0,123 g", "- 0,123 g"]:
            with self.subTest(raw=raw):
                result = parser.process(raw, settings)
                self.assertFalse(result.ok)

    def test_preset_round_trip_keeps_parser_sign_options(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "signs.json"
            settings = AppSettings.built_in_defaults()
            settings.parsing.normalize_sign = True
            settings.parsing.drop_plus_sign = True
            settings.parsing.drop_minus_sign = True
            settings.output.dry_run = True
            save_preset(path, "Signs", settings)
            loaded = load_preset(path)
            self.assertEqual(loaded.settings.serial.port, "COM6")
            self.assertTrue(loaded.settings.parsing.normalize_sign)
            self.assertTrue(loaded.settings.parsing.drop_plus_sign)
            self.assertTrue(loaded.settings.parsing.drop_minus_sign)
            self.assertTrue(loaded.settings.output.dry_run)
            self.assertEqual(loaded.settings.serial.eol, "\r\n")

    def test_output_sender_builds_unicode_text_inputs(self) -> None:
        sender = WindowsInputSender()
        inputs = sender.build_text_inputs("0.12")
        self.assertEqual(len(inputs), 8)
        self.assertEqual(inputs[0].ki.wVk, 0)
        self.assertEqual(inputs[0].ki.wScan, ord("0"))
        self.assertEqual(inputs[0].ki.dwFlags, KEYEVENTF_UNICODE)
        self.assertEqual(inputs[1].ki.dwFlags, KEYEVENTF_UNICODE | KEYEVENTF_KEYUP)

    def test_output_sender_uses_native_input_struct_size(self) -> None:
        sender = WindowsInputSender()
        size = ctypes.sizeof(type(sender.build_text_inputs("0")[0]))
        expected = 40 if ctypes.sizeof(ctypes.c_void_p) == 8 else 28
        self.assertEqual(size, expected)

    def test_output_sender_builds_utf16_surrogate_pairs_when_needed(self) -> None:
        sender = WindowsInputSender()
        inputs = sender.build_text_inputs("𝟘")
        self.assertEqual(len(inputs), 4)
        first_code_unit = int.from_bytes("𝟘".encode("utf-16-le")[:2], "little")
        second_code_unit = int.from_bytes("𝟘".encode("utf-16-le")[2:4], "little")
        self.assertEqual(inputs[0].ki.wScan, first_code_unit)
        self.assertEqual(inputs[2].ki.wScan, second_code_unit)

    def test_output_sender_post_action_inputs_use_virtual_keys(self) -> None:
        sender = WindowsInputSender()
        inputs = sender.build_post_action_inputs("down", [])
        self.assertIsNotNone(inputs)
        assert inputs is not None
        self.assertEqual(len(inputs), 2)
        self.assertEqual(inputs[0].ki.wVk, 0x28)
        self.assertEqual(inputs[1].ki.dwFlags, KEYEVENTF_KEYUP)
        self.assertIsNone(sender.build_post_action_inputs("unknown", []))

    def test_output_sender_builds_custom_sequence_inputs(self) -> None:
        sender = WindowsInputSender()
        inputs = sender.build_post_action_inputs(CUSTOM_SEQUENCE_ACTION, ["down", "tab"])
        self.assertIsNotNone(inputs)
        assert inputs is not None
        self.assertEqual(len(inputs), 4)
        self.assertEqual(inputs[0].ki.wVk, 0x28)
        self.assertEqual(inputs[2].ki.wVk, 0x09)

    def test_about_text_omits_author_name(self) -> None:
        text = about_text()
        self.assertIn("ScaleLogger 0.95", text)
        self.assertNotIn("Jakub Toczek", text)


if __name__ == "__main__":
    unittest.main()
