# Build ScaleLogger Native (Windows)

## Prerequisites
- Windows 10/11
- Visual Studio 2022 with Desktop C++ workload
- CMake 3.24+

## Configure
```powershell
cmake --preset windows-vs2022-x64
```

## Build (Release)
```powershell
cmake --build --preset windows-release
```

Expected executable location:
`out/build/windows-vs2022-x64/Release/ScaleLogger.exe`

## Run tests
```powershell
ctest --preset windows-test
```

## Create release bundle
From repository root:
```bat
ScaleLogger_build_release.bat
```

This produces:
- `release\ScaleLogger.exe`
- `release\SHA256SUMS.txt`
- `release\BUILD_MANIFEST_0.95.txt`

## First real runtime test pass
After successful build/test, run the focused manual runtime checklist:
- [docs/first_windows_runtime_test_checklist.md](docs/first_windows_runtime_test_checklist.md)

## Notes
- The repository no longer uses Python/PySide6/Nuitka for the main build/release path.
- The migration document is kept for behavior reference: `docs/migration_from_current_implementation.md`.
