# Build ScaleLogger Native (Windows)

## Requirements
- Visual Studio 2022 (Desktop C++)
- CMake 3.24+

## Configure
```powershell
cmake --preset windows-vs2022-x64
```

## Build
```powershell
cmake --build --preset windows-release
```

## Test
```powershell
ctest --preset windows-test
```

## Notes
- Native target executable: `ScaleLogger` (Win32 app).
- Tests validate parser/config/key-sequence logic.
