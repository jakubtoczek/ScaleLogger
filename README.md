# ScaleLogger

ScaleLogger is being migrated from Python/PySide6 to a native Windows C++20 Win32 application.

## Native rewrite baseline
- C++20 + CMake + Win32 dialog architecture (`src/`)
- Native serial stack via Win32 APIs (`src/serial`)
- Native keyboard injection via SendInput (`src/input`)
- JSON config/presets with nlohmann/json (`src/core`)
- Logging support with spdlog (`src/core`, `src/app`)
- Resource metadata via `.rc` (`resources/ScaleLogger.rc`)
- Native logic tests for parser/config/key-sequence (`tests/`)

For migration details and behavior parity notes, see `docs/migration_from_current_implementation.md`.
