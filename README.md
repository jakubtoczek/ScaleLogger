# ScaleLogger (Native Win32)

ScaleLogger is a Windows-only native C++20 desktop utility for reading serial scale output and injecting values into the currently focused window.

## Status
This repository now uses the **native C++/Win32/CMake** implementation as the primary code path.

Legacy Python/PySide6/Nuitka runtime/build files were removed from the active build path.

## Tech stack
- C++20
- Win32 APIs (GUI, serial, SendInput)
- CMake
- MSVC / Visual Studio 2026 x64

## Repository layout
- `src/` native application code
- `tests/` native tests (parser/config/key-sequence)
- `resources/` icon + version resource script
- `docs/migration_from_current_implementation.md` behavior-compatibility notes
- `tools/` release helper scripts
- `packaging/` release packaging notes

## Build on Windows
See [BUILD_WINDOWS.md](BUILD_WINDOWS.md).

Quick start:
```powershell
cmake --preset windows-vs2026-x64
cmake --build --preset windows-release
ctest --preset windows-test
```

Prerequisites:
- Visual Studio 2026 (MSVC x64 toolchain)
- CMake available in `PATH`

## Repo-local release wrapper
For a fresh-clone reproducible release package, run:

```bat
ScaleLogger_build_release.bat
```

The wrapper always configures/builds/tests in Release first, then produces:
- `release\ScaleLogger.exe`
- `release\SHA256SUMS.txt`
- `release\BUILD_MANIFEST_<version>.txt`

## Configuration JSON format
ScaleLogger reads and writes a JSON config file (`ScaleLogger.config.json`) with fields such as:
- `config_folder`, `config_file_name`
- `presets_folder`, `logs_folder`
- `log_file_pattern`
- `log_mode` (`none`, `single_file`, `per_session`)
- `connect_on_startup`
- `dark_mode` (experimental; default is `false`)
- `startup_mode` and preset name fields
- `standalone_mode`
- optional serial dropdown option arrays: `baud_rates`, `data_bits_options`, `parity_options`, `stop_bits_options`

To keep schema consistency, config files also carry serial/parsing/output fields (same structure used by presets), including `custom_sequence` and `eol`.
Presets may also include app-level fields (`config_folder`, `logs_folder`, `log_mode`, etc.); unknown/extra fields remain backward-compatible.

Precedence rule:
1. Config provides app-level defaults and serial/output defaults.
2. When a startup/selected preset is loaded, preset values override runtime serial/output behavior.

Path rule:
- `config_folder`, `presets_folder`, and `logs_folder` are treated as runtime-resolved filesystem paths.
- Absolute paths are used as-is.
- Relative paths are resolved against the app data root (`%USERPROFILE%\ScaleLogger` on Windows).

## Python compatibility notes
Preset loading is backward compatible with legacy Python-era keys:
- `drop_plus_sign` maps to `preserve_plus_sign` behavior
- `normalize_sign` is still honored
- `eol` values `\\r\\n`, `\\n`, and `\\r` are interpreted as CRLF/LF/CR
- `custom_sequence` is read from JSON arrays (for example `["down","down","right"]`)

When compatibility mapping is applied, a runtime log line indicates it.

## Logging modes
- **No file logging** (`log_mode: "none"`): UI log only.
- **Single file** (`log_mode: "single_file"`): appends to one log file.
- **Per session** (`log_mode: "per_session"`): creates a timestamped file using `log_file_pattern`.

Runtime file logging flushes each line and emits a one-time visible error if file writes fail.

## Standalone mode
When `standalone_mode` is `true`, ScaleLogger avoids writing config, preset, and log files.  
This mode is intended for restricted or temporary environments where no local file output is desired.

Behavior notes:
- If no config file exists, app still loads runtime defaults (or `default_config.json`) for current session.
- In standalone mode, settings still apply in-memory, but file persistence is intentionally skipped.

## Portable default paths
`default_config.json` uses `%LOCALAPPDATA%` placeholders for folder defaults.  
At runtime these placeholders are expanded to the active user profile location so builds are portable across different Windows accounts.

## Release outputs
Use `ScaleLogger_build_release.bat` to create a native release folder with:
- `ScaleLogger.exe`
- `SHA256SUMS.txt`
- `BUILD_MANIFEST_0.96.txt`

## First runtime validation
Use [docs/first_windows_runtime_test_checklist.md](docs/first_windows_runtime_test_checklist.md) for the exact first real Windows runtime test pass (build -> launch -> COM/serial -> parse/inject -> after-send actions -> release script).

## Compatibility/migration docs
`docs/migration_from_current_implementation.md` captures the behavior mapping from the previous implementation and known intentional differences.

## License
MIT (`LICENSE`).
