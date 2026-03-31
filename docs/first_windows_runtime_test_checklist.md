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
- startup full-config load before auto-connect
- release script consistency

## Prerequisites
- Windows 10/11 machine with a real serial device (or stable virtual COM pair).
- Visual Studio 2026 Desktop C++ workload.
- CMake 3.24+ in `PATH`.
- A target app/window where injected text/keys can be observed (e.g., Notepad).

## 1) Clean native build + tests
From repo root in PowerShell:

```powershell
cmake --preset windows-vs2026-x64
cmake --build --preset windows-release
ctest --preset windows-test
```

Pass criteria:
- Configure/build/test complete with no errors.
- `out/build/windows-vs2026-x64/Release/ScaleLogger.exe` exists.

Expected Results:
- CMake configure and build complete without fatal errors.
- `ctest` reports all tests passed.
- Native executable is present at the expected output path.

## 2) Prepare runtime data folder
Runtime data lives under:
- `%USERPROFILE%\ScaleLogger`

Ensure these exist (create if missing):
- `%USERPROFILE%\ScaleLogger\ScaleLogger.config.json`
If `%USERPROFILE%\\ScaleLogger\\ScaleLogger.config.json` is missing, startup fallback `default_config.json` is resolved from the executable directory.

Expected Results:
- Config file is present under `%USERPROFILE%\ScaleLogger` and uses the intended serial/parsing/output values for this test run.

For early startup crashes before UI logging appears, inspect `%TEMP%\ScaleLogger_fatal.log`.

## 3) Validate startup full-config load before auto-connect
In `ScaleLogger.config.json`, set:
- `connect_on_startup: true`
- required serial/parsing/output values for your target device

Run app from terminal:

```powershell
out/build/windows-vs2026-x64/Release/ScaleLogger.exe
```

Pass criteria (terminal log order):
1. Startup config load message appears first.
2. Auto-connect log appears after config load.
3. Port in auto-connect log matches configured port.

Expected Results:
- Observable startup order is config load -> auto-connect attempt.
- Auto-connect target port matches the startup config file.

## 4) Validate COM discovery and manual connect/disconnect
- In a test helper (or temporary callsite), invoke `ScanComPorts()` and verify expected COM ports are listed.
- In app window, click **Connect** then **Disconnect**.

Pass criteria:
- Device COM port appears in scan results.
- Connect succeeds on expected COM port.
- Disconnect is clean (no hang/crash).

Expected Results:
- Known active COM ports are listed after scan.
- Connect produces a successful open log on the selected port.
- Disconnect returns app to idle state without freeze/crash.

## 5) Validate serial receive -> parser -> injection
- Keep a target input field focused (e.g., Notepad text area).
- Send representative device lines, including valid and invalid lines.

Pass criteria:
- Valid line: parsed value is injected into focused target.
- Invalid line: app logs parse rejection and does not inject garbage.
- No dropped app responsiveness while receiving lines.

Expected Results:
- Valid payloads produce expected parsed text in the focused target (Notepad/Excel cell input).
- Malformed payloads are rejected with parse logs and no unintended text injection.
- App remains responsive during repeated receive events.

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

Expected Results:
- `down/right/enter/tab` move/confirm focus exactly once per send.
- `none` injects text without additional key movement.
- `custom_sequence` applies tokens in order, including correct behavior for `F10/F11/F12`.

## 7) Validate release script consistency
From repo root in `cmd.exe`:

```bat
ScaleLogger_build_tagged_release.bat
```

Pass criteria:
- Script exits success.
- `release\ScaleLogger_<buildtag>.exe` exists.
- `release\SHA256SUMS.txt` exists and references `ScaleLogger_<buildtag>.exe`.
- `release\BUILD_MANIFEST_<version>.txt` exists (version from `CMakeLists.txt`).
- Manifest includes runtime combo option arrays (`baud_rates`, `data_bits_options`, `parity_options`, `stop_bits_options`).

Expected Results:
- Release script completes end-to-end without manual intervention.
- Release folder contains executable, checksum file, and versioned manifest with expected names.

## 8) Record verdict
Mark readiness:
- **Ready for expanded device/runtime testing** if all sections above pass.
- **Not ready** if any blocker in build, launch, COM scan, serial receive/dispatch, parse->inject, after-send action, startup config/auto-connect, or release script.
- If failure happens before normal UI logging initializes, attach `%TEMP%\ScaleLogger_fatal.log` to the test notes.

## Known non-blocking items for this phase
- Config JSON handling uses embedded lightweight parser/writer code.
- Runtime UI is intentionally minimal (test-shell level).
- Additional real-device serial edge-case validation is still required.

## Test Result Recording
Test Result Summary
- Build: PASS / FAIL
- App launch: PASS / FAIL
- COM discovery: PASS / FAIL
- Serial receive pipeline: PASS / FAIL
- Injection behavior: PASS / FAIL
- After-send actions: PASS / FAIL
- Release script: PASS / FAIL
- Notes / observed issues:
