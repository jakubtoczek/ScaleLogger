# ScaleLogger (Native Win32)

ScaleLogger is a Windows-only native C++20 desktop utility for reading serial scale output and injecting values into the currently focused window.

## Build

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Runtime behavior

- Deterministic startup: defaults -> user config -> fallback `default_config.json`.
- Explicit connect/disconnect (no hidden auto-connect side effects).
- Pipeline: `Serial -> Parser -> Validation -> DryRun gate -> Injector -> PostAction`.
- Force-disable serial with: `SCALELOGGER_FORCE_NO_SERIAL=1`.

## Config

Single source of truth is `ScaleLogger.config.json` under `%USERPROFILE%\ScaleLogger`.
Relative paths are resolved against the data root and `%USERPROFILE%` is expanded.
