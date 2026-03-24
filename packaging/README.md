# Packaging baseline

- Build with CMake/VS2022 x64.
- Ship native `ScaleLogger.exe` + required runtime dependencies.
- Avoid onefile temp self-extraction packaging.
- Attach `SHA256SUMS.txt` and `BUILD_MANIFEST.md` to releases.
