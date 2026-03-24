# First Real Windows Runtime Test Checklist

This checklist is the **go/no-go** for the first native Win32 runtime validation pass.

## Scope (blocker-level only)
Validate these paths end-to-end:
- native configure/build/test
- app launch
- COM discovery
- serial connect + receive + line dispatch
- parse -> inject
- after-send action (including custom sequence)
- startup preset loading before auto-connect
- release script consistency

## Prerequisites
- Windows 10/11 machine with a real serial device (or stable virtual COM pair).
- Visual Studio 2022 Desktop C++ workload.
- CMake 3.24+ in `PATH`.
- A target app/window where injected text/keys can be observed (e.g., Notepad).

## 1) Clean native build + tests
From repo root in PowerShell:

```powershell
cmake --preset windows-vs2022-x64
cmake --build --preset windows-release
ctest --preset windows-test
```

Pass criteria:
- Configure/build/test complete with no errors.
- `out/build/windows-vs2022-x64/Release/ScaleLogger.exe` exists.

## 2) Prepare runtime data folder
Runtime data lives under:
- `%LOCALAPPDATA%\ScaleLogger`

Ensure these exist (create if missing):
- `%LOCALAPPDATA%\ScaleLogger\ScaleLogger.config.json`
- `%LOCALAPPDATA%\ScaleLogger\presets\`

Use a preset file in `presets\` that matches your device serial parameters and parse/output settings.

## 3) Validate startup preset load before auto-connect
In `ScaleLogger.config.json`, set:
- `connect_on_startup: true`
- `startup_mode: "specific_preset"` (or `"last_used_preset"`)
- matching preset name field (`startup_preset_name` or `last_used_preset_name`)

Run app from terminal:

```powershell
out/build/windows-vs2022-x64/Release/ScaleLogger.exe
```

Pass criteria (terminal log order):
1. Startup preset load message appears first.
2. Auto-connect log appears after preset load.
3. Port in auto-connect log matches preset-selected port.

## 4) Validate COM discovery and manual connect/disconnect
- In a test helper (or temporary callsite), invoke `ScanComPorts()` and verify expected COM ports are listed.
- In app window, click **Connect** then **Disconnect**.

Pass criteria:
- Device COM port appears in scan results.
- Connect succeeds on expected COM port.
- Disconnect is clean (no hang/crash).

## 5) Validate serial receive -> parser -> injection
- Keep a target input field focused (e.g., Notepad text area).
- Send representative device lines, including valid and invalid lines.

Pass criteria:
- Valid line: parsed value is injected into focused target.
- Invalid line: app logs parse rejection and does not inject garbage.
- No dropped app responsiveness while receiving lines.

## 6) Validate after-send actions and custom sequence
Run these output modes against a focused target:
- `down`
- `right`
- `enter`
- `tab`
- `none`
- `custom_sequence` (include `f10`, `f11`, `f12` in sequence)

Pass criteria:
- Built-in post-actions emit expected key behavior.
- Custom sequence tokens execute in order.
- `F10/F11/F12` tokens execute correctly when provided in sequence.

## 7) Validate startup mode variants
Test each startup mode once:
- `specific_preset`
- `last_used_preset`
- no matching preset (missing file)

Pass criteria:
- Existing preset modes load expected preset.
- Missing preset does not crash; app still launches.
- Auto-connect behavior follows loaded/default runtime settings.

## 8) Validate release script consistency
From repo root in `cmd.exe`:

```bat
ScaleLogger_build_release.bat
```

Pass criteria:
- Script exits success.
- `release\ScaleLogger.exe` exists.
- `release\SHA256SUMS.txt` exists and references `ScaleLogger.exe`.
- `release\BUILD_MANIFEST_0.95.txt` exists.

## 9) Record verdict
Mark readiness:
- **Ready for expanded device/runtime testing** if all sections above pass.
- **Not ready** if any blocker in build, launch, COM scan, serial receive/dispatch, parse->inject, after-send action, startup preset/auto-connect, or release script.

## Known non-blocking items for this phase
- Config/preset JSON handling uses manual string parsing.
- Runtime UI is intentionally minimal (test-shell level).
- Additional real-device serial edge-case validation is still required.
