# Migration from Python/PySide6 implementation to native Win32 C++

## Source-of-truth behavior inspected
The prior implementation behavior was derived from:
- `app/parser.py`
- `app/config.py`
- `app/presets.py`
- `app/serial_manager.py`
- `app/output_sender.py`
- `app/ui/main_window.py`

## Behavior preserved in rewrite baseline
- TR-602-like default serial settings (COM6/1200/7/O/1/1.0s).
- Parsed and raw handling model with trimming/suffix/sign normalization.
- Startup `connect_on_startup` setting persisted in config.
- Reconnect only for connection-critical serial fields.
- Runtime-safe settings applied without forced disconnect.
- Compact log mode default (`line_log_mode: compact`).
- Stale-buffer handling at connect time using native Win32 APIs.

## Required updates explicitly carried forward
1. **Auto-connect on startup** is enabled by default and logs intent.
2. **No disconnect for runtime-safe settings** via `SerialSettingsRequireReconnect`.
3. **Compact logging default** with optional verbose mode retained in schema.
4. **Stale buffered serial data purge** sequence before marking connection active:
   - open + configure
   - purge RX
   - 100ms settle delay
   - drain immediate bytes
   - final RX purge
   - then mark connected
5. **Startup log density cleanup** by keeping startup logging concise in controller.
6. **About cleanup** retains version/repo/license/assistant note with no author name.

## Intentional differences in this commit
- This commit introduces a native C++ architecture baseline and core-tested modules.
- Full production-complete Win32 dialogs/event wiring from Python parity are scaffolded and documented, with the core parser/config/sequence logic implemented and tested.

## Config and preset compatibility notes
- Config schema keeps keys:
  `presets_folder`, `logs_folder`, `log_mode`, `line_log_mode`, `connect_on_startup`, `startup_mode`, `startup_preset_name`, `last_used_preset_name`.
- Preset flat schema loading keeps the existing serial/parsing fields (`port`, `baudrate`, `databits`, `parity`, `stopbits`, `timeout`, `eol`, etc.).

## Release/tooling direction
- CMake + VS2022 x64 presets.
- Windows `.rc` resource included for icon/version metadata.
- Packaging helper placeholders added under `tools/` and `packaging/`.
