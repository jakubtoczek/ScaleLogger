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

## Create release bundle
From repository root:
```bat
ScaleLogger_build_tagged_release.bat
```
Optional:
```bat
ScaleLogger_build_tagged_release.bat [build_tag] [keep]
```
Default behavior removes `out\` after successful packaging. Add `keep` to preserve `out\` and other intermediate build artifacts for inspection/debugging.

This produces:
- `release\ScaleLogger_<buildtag>.exe`
- `release\SHA256SUMS.txt`
- `release\BUILD_MANIFEST_<version>.txt` (version comes from `CMakeLists.txt`)

## Notes
- If your shell exports `RC=0`/`RC=1`, clear it before configure (`set RC=`). The release script and configure preset now do this defensively.
- For crashes before the UI log appears, check `%TEMP%\\ScaleLogger_fatal.log` (early startup traces + fatal exception breadcrumbs).
- Startup fallback `default_config.json` is read from the executable directory when user config is missing.
