# Build ScaleLogger Native (Windows)

## Prerequisites
- Windows 10/11
- Visual Studio 2026 with Desktop C++ workload
- CMake 3.24+

## Configure
```powershell
cmake --preset windows-vs2026-x64
```

## Build (Release)
```powershell
cmake --build --preset windows-release
```

Expected executable location:
`out/build/windows-vs2026-x64/Release/ScaleLogger.exe`

## Run tests
```powershell
ctest --preset windows-test
```

## Create release bundle
From repository root:
```bat
ScaleLogger_build_tagged_release.bat
```

This produces:
- `release\ScaleLogger_<buildtag>.exe`
- `release\SHA256SUMS.txt`
- `release\BUILD_MANIFEST_<version>.txt` (version comes from `CMakeLists.txt`)

`ScaleLogger_build_tagged_release.bat` is the authoritative repo-local tagged release wrapper. External helper scripts such as `extbuild.bat` are convenience wrappers outside this repo and are not the source of truth.

## First real runtime test pass
After successful build/test, run the focused manual runtime checklist:
- [docs/first_windows_runtime_test_checklist.md](docs/first_windows_runtime_test_checklist.md)

## Notes
- The repository no longer uses Python/PySide6/Nuitka for the main build/release path.
- The migration document is kept for behavior reference: `docs/migration_from_current_implementation.md`.
- If your shell exports `RC=0`/`RC=1`, clear it before configure (`set RC=`). The release script and configure preset now do this defensively.
- For crashes before the UI log appears, check `%TEMP%\\ScaleLogger_fatal.log` (early startup traces + fatal exception breadcrumbs).
- Startup fallback `default_config.json` is read from the executable directory when user config is missing.
