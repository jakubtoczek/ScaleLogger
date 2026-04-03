# Packaging baseline (native)

Primary release flow is Windows-native:
1. Configure/build/test via CMake presets.
2. Copy `ScaleLogger.exe` into `release/`.
3. Generate `SHA256SUMS.txt`.
4. Generate `BUILD_MANIFEST_<version>.txt`.

Use `ScaleLogger_build_tagged_release.bat` for the default workflow.

No Python/Nuitka onefile packaging is used in the active release path.
