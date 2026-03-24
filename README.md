# ScaleLogger (Native Win32)

ScaleLogger is a Windows-only native C++20 desktop utility for reading serial scale output and injecting values into the currently focused window.

## Status
This repository now uses the **native C++/Win32/CMake** implementation as the primary code path.

Legacy Python/PySide6/Nuitka runtime/build files were removed from the active build path.

## Tech stack
- C++20
- Win32 APIs (GUI, serial, SendInput)
- CMake
- MSVC / Visual Studio 2022 x64

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
cmake --preset windows-vs2022-x64
cmake --build --preset windows-release
ctest --preset windows-test
```

## Release outputs
Use `ScaleLogger_build_release.bat` to create a native release folder with:
- `ScaleLogger.exe`
- `SHA256SUMS.txt`
- `BUILD_MANIFEST_0.95.txt`

## Compatibility/migration docs
`docs/migration_from_current_implementation.md` captures the behavior mapping from the previous implementation and known intentional differences.

## License
MIT (`LICENSE`).
