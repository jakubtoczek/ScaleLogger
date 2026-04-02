# ScaleLogger 0.97spec

ScaleLogger is a lightweight **Windows-only** desktop utility for receiving serial data from a laboratory balance and sending the resulting value to the **currently focused window**.

Repository URL: `github.com/jakubtoczek/ScaleLogger`

- Windows desktop utility
- Sends text to the active window with native keyboard injection
- No direct Excel / LibreOffice integration
- Minimal UI with presets, settings, status, and logs
- MIT-licensed

> Development assisted by OpenAI ChatGPT and Codex.

## Recommended environment

- Python **3.12 64-bit** for development and packaging
- `requirements.txt` for source-run/runtime dependencies
- `requirements-build.txt` for build/package dependencies
- Transitional packaging on this branch uses Python + PySide6 + Nuitka

## What the app does

ScaleLogger reads line-based serial output from a balance, parses the line according to your selected settings, and sends the result to the currently focused window.

After sending the value, the default after-send action is `Down`, and the app can optionally send one of these keys:

- `Down`
- `Right`
- `Enter`
- `Tab`
- `Nothing`

The app is designed for spreadsheet-style workflows, but it does **not** integrate directly with Excel or LibreOffice APIs.

## Focus and privilege notes

ScaleLogger uses Windows keyboard injection, so the destination cell/window must already be focused before the balance sends data.

For reliable injection:

- keep Excel, Notepad, LibreOffice Calc, or the other target window focused
- avoid running the target app elevated if ScaleLogger is not elevated
- ideally run both apps at the same privilege level

Windows note:
- The one-file executable extracts internal runtime components temporarily at launch.
- Some antivirus tools may scan these extracted files.
- For local testing, placing the application in a dedicated folder and excluding that folder from scans may help.

## Version

Current repository/app version: **0.97spec**.

## Config vs preset format

### Built-in defaults

The application always includes built-in fallback defaults in Python code. These are internal defaults and are **not** shown as fake presets in the preset dropdown.

### `ScaleLogger.config.json`

`ScaleLogger.config.json` is the **app-level** config file and stores:

- `presets_folder`
- `logs_folder`
- `log_mode`
- `line_log_mode`
- `connect_on_startup`
- `startup_mode`
- `startup_preset_name`
- `last_used_preset_name`

### Preset JSON files

Preset files are separate flat `*.json` files in the presets folder. They store the actual serial, parsing, and output settings.

Important behavior:

- only actual `*.json` preset files appear in the preset dropdown
- built-in defaults do not appear as a fake preset
- the preset dropdown only shows a selected preset when that preset is actually active
- malformed or obsolete preset files are rejected and logged
- duplicate preset names are rejected and logged

## Example templates included in the repo

The repository includes text-only templates and helpers:

- `ScaleLogger.config.json.example`
- `presets/TR-602.json.example`
- `ScaleLogger_build_tagged_release.bat`
- `generate_sha256.bat`

The config example intentionally keeps `logs_folder` and `presets_folder` as relative values. Those values resolve under the persistent per-user ScaleLogger data directory rather than the repository root or a temp extraction folder.

## Persistent app data location

User data is stored in a stable per-user folder instead of any one-file extraction or runtime temp directory. The app derives this location from normal user-home path APIs (`Path.home()`), so it does not depend on hardcoded English Windows folder names and remains safe on localized Windows installations.

```text
C:\Users\<current_user>\ScaleLogger\
  ScaleLogger.config.json
  logs\
  presets\
```

Default derived paths:

- app config: `C:\Users\<current_user>\ScaleLogger\ScaleLogger.config.json`
- presets folder: `C:\Users\<current_user>\ScaleLogger\presets\`
- logs folder: `C:\Users\<current_user>\ScaleLogger\logs\`

## Startup behavior

The simple settings UI exposes **Startup preset** instead of the full startup-mode matrix.

- if Startup preset is set, that preset is used on startup
- if Startup preset is left empty, ScaleLogger reuses the last used preset by default
- if no last used preset is available, built-in defaults are used
- the underlying `startup_mode` field still exists in `ScaleLogger.config.json` for manual editing if needed

`Connect on startup` is enabled by default. When it is enabled, ScaleLogger finishes loading config/preset state and then starts the serial connection in the background automatically.

## Serial settings usability in v0.97spec

### COM port scanning

The Serial tab now includes **Scan Ports**. The dropdown is refreshed in place, manual entry remains available, and the button provides short feedback such as `Scanning…` / `Updated` so the user knows the scan ran.

- detected ports keep concise labels in the dropdown
- manual entry is still supported
- Windows-style normalization still applies (`6` becomes `COM6`)

### Test Receive

The Serial tab also includes **Test Receive**, a compact troubleshooting dialog.

It shows:

- the currently selected serial settings summary
- `Start` / `Stop` controls
- a status message such as waiting, receiving, no data yet, or decode issue
- a scrolling raw receive log

Use it to confirm whether readable lines are arriving from the balance before enabling the main capture workflow. It does **not** send anything to the active window.

## Output settings

The Output tab is organized into:

- **Value formatting**: parsed/raw mode, whitespace cleanup, suffix handling, sign handling, numeric validation
- **After-send**: `Down`, `Right`, `Enter`, `Tab`, `Nothing`, or `Custom sequence`

The behavior is explicit:

1. ScaleLogger sends the value to the currently focused window
2. then it sends the selected after-send action

When **Custom sequence** is selected, a compact editor appears below the dropdown. Use `Capture Key` to append a key, then `Remove Last` or `Clear` to adjust the sequence. The stored format remains a simple ordered list of key tokens in presets/config-backed settings.

## Logging behavior

Persistent logs always live in the ScaleLogger logs folder. The settings UI offers three recording modes:

- `No log`
- `Single log file (append)`
- `New log file for each session` *(default)*

The GUI log remains visible for the current session even when persistent file logging is disabled.

Compact capture logging is the default. Successful captures appear on one line, for example `Raw: ' 0.00000 g ' -> Parsed: '0.00000'`. If you prefer the older two-line behavior, set `line_log_mode` to `verbose` in `ScaleLogger.config.json`.

## Parsing behavior

ScaleLogger supports:

- **parsed mode** for strict numeric capture with optional cleanup
- **raw mode** for sending the processed incoming line directly

Parsed mode behavior:

- trims surrounding whitespace when enabled
- supports spaced sign normalization such as `-  0.00123 g -> -0.00123`
- can preserve or drop a leading `+`
- rejects malformed multi-sign inputs such as `++0.1 g` and `+-0.1 g`
- strips an explicit configured suffix or, if no explicit suffix is set, a narrow set of common balance unit suffixes
- rejects junk/status text and decimal-comma inputs

Malformed parsed lines are not sent to the active window and are logged as warnings.

## Runtime icon handling

This repository does **not** include `icon.ico`.

If you provide `icon.ico` manually at the repository root before building, ScaleLogger will try to use it for:

- source runs
- standalone builds
- one-file builds (as practical for the runtime extraction environment)

Missing `icon.ico` is safe and non-fatal at runtime.

## About action

The app includes a minimal About action showing:

- app name and version
- repository URL
- MIT license
- OpenAI/Codex acknowledgement

## Requirements files

The repository keeps the dependency split intentionally:

- `requirements.txt` = runtime/source-run dependencies
- `requirements-build.txt` = build/package dependency list

## Run from source

```powershell
py -3.12 -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python main.py
```

## Build on Windows

### One-file build helper

```powershell
ScaleLogger_build_tagged_release.bat --tag v0.97spec
```

This helper creates a fresh `.venv64` by default, verifies the final one-file executable before hashing it, writes tag-specific checksum and manifest files (`SHA256SUMS_<tag>.txt`, `BUILD_MANIFEST_<tag>.txt`), and removes temporary Nuitka artifact folders from `release\` after a successful build. Pass `--keep-venv` for a faster rebuild that reuses the existing environment.

Recommended notes:

- use Python 3.12 64-bit
- use `py -3.12-64` for packaging
- packaged executables do not require Python on the target machine
- one-file builds may be scanned aggressively on some systems
- signing / SignPath is planned for 1.0
- for local trusted testing, see `BUILD_WINDOWS.md` for Windows Defender guidance

See `BUILD_WINDOWS.md` for the detailed 64-bit Windows build workflow.

## Branch status note

The product design target is a native Win32 / C++20 / CMake implementation. This repository snapshot remains a transitional Python/PySide6 codebase and is not yet aligned to that native implementation target.

## License

ScaleLogger is released under the **MIT License**. See `LICENSE`.
