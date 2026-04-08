# ScaleLogger (Native Win32)

ScaleLogger is a Windows-only native C++20 desktop utility for reading serial scale output and injecting values into the currently focused window.

## Tech stack
- C++20
- Win32 APIs (GUI, serial, SendInput)
- CMake
- MSVC / Visual Studio 2026 x64

## Repository layout
- `src/` native application code
- `resources/` icon + version resource script
- `tools/` release helper scripts

## Build on Windows
See [BUILD_WINDOWS.md](BUILD_WINDOWS.md).

Quick start:
```powershell
cmake --preset windows-vs2026-x64
cmake --build --preset windows-release
```

Prerequisites:
- Visual Studio 2026 (MSVC x64 toolchain)
- CMake available in `PATH`

## Release bundle generation
For a fresh-clone reproducible release package, run:

```bat
ScaleLogger_build_tagged_release.bat
```
Optional:
```bat
ScaleLogger_build_tagged_release.bat [build_tag] [keep]
```
Default behavior removes `out\` after successful packaging. Add `keep` to preserve `out\` and intermediate build artifacts.

The wrapper configures/builds in Release first, then produces:
- `release\ScaleLogger_<buildtag>.exe`
- `release\SHA256SUMS.txt`
- `release\BUILD_MANIFEST_<version>.txt`

## Configuration overview
ScaleLogger reads and writes a JSON config file (`ScaleLogger.config.json`) with fields such as:
- `config_folder`, `config_file_name`
- `logs_folder`
- `log_file_pattern`
- `log_mode` (`none`, `single_file`, `per_session`)
- `connect_on_startup`
- `dark_mode` (main-window dark styling toggle; default is `false`)
- `enable_startup_trace` (controls early TRACE lines in fatal forensics; default `true`)
- `enable_fatal_log_file` (controls writing `%TEMP%\\ScaleLogger_fatal.log`; default `true`)
- `show_crash_dialog` (shows copy-friendly crash dialog on fatal crash; default `true`)
- `include_trace_in_crash_dialog` (includes recent fatal trace text in crash dialog; default `true`)
- optional serial dropdown option arrays: `baud_rates`, `data_bits_options`, `parity_options`, `stop_bits_options`

To keep schema consistency, config files also carry serial/parsing/output fields (including `custom_sequence` and `eol`) in the same single full-config file.
The single configuration file is the runtime source of truth for app-level and serial/parsing/output behavior.
When `post_action` is `custom_sequence`, the intended supported tokens are:
`enter`, `tab`, `up`, `down`, `left`, `right` (optional: `esc`, `space`).

Path rule:
- `config_folder` and `logs_folder` are treated as runtime-resolved filesystem paths.
- Absolute paths are used as-is.
- Relative paths are resolved against the app data root (prefer `%USERPROFILE%\ScaleLogger` on Windows).
- If user config is missing, startup fallback `default_config.json` is resolved from the executable directory (not from the process working directory).

## Logging modes
- **No file logging** (`log_mode: "none"`): UI log only.
- **Single file** (`log_mode: "single_file"`): appends to one log file.
- **Per session** (`log_mode: "per_session"`): creates a timestamped file using `log_file_pattern`.

Runtime file logging flushes each line and emits a one-time visible error if file writes fail.

## Logging and fatal diagnostics

ScaleLogger uses a dedicated early-startup and crash log:

- Before UI logging is initialized, startup traces are written to:
  `%TEMP%\\ScaleLogger_fatal.log`
- This file is also used for fatal crash diagnostics (via `SetUnhandledExceptionFilter`)
- Use this file first if the app exits or crashes before the in-app log appears

These diagnostic traces are separate from normal runtime/session logging.

On fatal crashes:
- A small native crash dialog is shown (Copy / Close)
- If dialog creation fails, a MessageBox fallback is used
- The report includes:
  - exception details (when available)
  - startup-crash indicator
  - path to `%TEMP%\\ScaleLogger_fatal.log`
  - optional recent trace lines

Configuration:
- `enable_startup_trace` → enables early startup tracing
- `enable_fatal_log_file` → controls writing the fatal log file
- `show_crash_dialog` → enables crash dialog
- `include_trace_in_crash_dialog` → embeds recent trace text in dialog

You can disable startup tracing with:
`enable_startup_trace=false`

## Portable default paths
`default_config.json` uses `%USERPROFILE%` placeholders for folder defaults.  
At runtime these placeholders are expanded to the active user profile location so builds are portable across different Windows accounts.

## Release outputs
Use `ScaleLogger_build_tagged_release.bat` to create a native release folder with:
- `ScaleLogger_<buildtag>.exe`
- `SHA256SUMS.txt`
- `BUILD_MANIFEST_<version>.txt` (version derived from `CMakeLists.txt`)

Release manifest includes runtime combo option arrays sourced from `default_config.json`:
- `baud_rates`
- `data_bits_options`
- `parity_options`
- `stop_bits_options`

## License
MIT (`LICENSE`).
