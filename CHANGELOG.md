# Changelog

## 0.96 - 2026-03-24

- cleaned repository to align with the native C++20/Win32/CMake baseline
- removed obsolete Python/PySide6/Nuitka runtime and packaging files from active branch
- updated README, Windows build instructions, and release script to the native build/release workflow
- updated build-manifest template for native compiler/CMake metadata

## 0.95 - 2026-03-20

- added COM-port scanning and a compact Test Receive dialog in Serial settings to make serial troubleshooting easier without affecting the main capture flow
- simplified the settings UI by grouping parsing/output options, switching baud/timeout to editable dropdowns, simplifying startup-preset behavior, adding persistent log recording modes, and adding a compact custom after-send key-sequence editor
- improved runtime icon handling for source / standalone / one-file use when `icon.ico` is available, while keeping missing icons safe and non-fatal
- polished the release workflow with verified one-file output, a corrected `BUILD_MANIFEST_0.95.txt`, standard `SHA256SUMS.txt`, safer cleanup of temporary Nuitka artifact folders, and clearer Defender/SmartScreen testing guidance

## 0.94 - 2026-03-20

- fixed the Windows text injection layer to use a Unicode `SendInput` path for ordinary text and clearer logging for text-vs-post-action failures
- updated the documentation for focus/privilege expectations, SHA256 generation, and build-manifest recording for packaged artifacts
- added `BUILD_MANIFEST_TEMPLATE.md`, `generate_sha256.bat`, and `ScaleLogger_build_release.bat` to support a cleaner v0.94 release workflow with standard `SHA256SUMS.txt` output and a generated build manifest

## 0.93 - 2026-03-20

- renamed the app to `ScaleLogger`, updated user-visible metadata/UI/About text, moved user data to the persistent per-user `ScaleLogger` folder, improved first-run config/log/preset behavior, normalized Windows numeric serial ports to `COMx`, and made the default post-write action `Down`

## 0.92 - 2026-03-19

- updated the built-in serial default port and the included `TR-602` example preset from COM5/legacy assumptions to `COM6`

## 0.91 - 2026-03-19

- finalized the repository around a cleaner flat preset schema, stricter parser sign handling, honest preset UI state, and explicit Python 3.12 64-bit build guidance
- follow-up review fixes: built-in serial defaults now use a real CRLF line ending and duplicate preset names are skipped with warnings instead of colliding silently

## 0.9 - 2026-03-19

- finalized the repository for public submission with versioned app metadata, a lightweight About action, MIT licensing, Windows build/docs cleanup, and example configuration templates
