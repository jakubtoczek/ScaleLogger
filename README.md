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
- `startup_mode` and preset name fields
- `standalone_mode`

Preset JSON files store serial, parsing, and output behaviors, including `custom_sequence` arrays and `eol`.

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
