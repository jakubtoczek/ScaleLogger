# Building ScaleLogger on Windows

ScaleLogger is a **Windows-only** PySide6 desktop application. The recommended build target for this repository is **Python 3.12 64-bit** using the Windows launcher command `py -3.12-64` and a dedicated `.venv64` virtual environment.

Repository URL: `github.com/jakubtoczek/ScaleLogger`

## Recommended target

- Windows 10 or Windows 11
- Python **3.12 64-bit**
- `py -3.12-64`
- `.venv64` local build environment
- manual `icon.ico` placement at the repository root if you want the icon embedded

> Older 32-bit packaging approaches are not recommended for this repository.

## 1. Open a local project folder

Example:

```text
C:\Projects\ScaleLogger
```

## 2. Create the 64-bit build environment manually (optional)

```powershell
py -3.12-64 -m venv .venv64
.\.venv64\Scripts\Activate.ps1
```

## 3. Install runtime/source dependencies

```powershell
pip install -r requirements.txt
```

## 4. Install build dependencies

```powershell
pip install -r requirements-build.txt
```

## 5. Place `icon.ico` manually if desired

`icon.ico` is **not** included in this repository.

If you want a custom Windows executable icon, place a real `icon.ico` file manually at the repository root before building:

```text
C:\Projects\ScaleLogger\icon.ico
```

Missing `icon.ico` is safe at runtime, but the build scripts intentionally fail early if the icon file is absent because they embed it in the packaged executable.

## 6. Run from source

```powershell
python main.py
```

## 7. Release helper build (one-file + checksum + manifest)

```powershell
ScaleLogger_build_release.bat
```

This helper script:

- checks for `py -3.12-64`
- creates a fresh `.venv64` by default
- accepts `--keep-venv` for a faster rebuild that reuses the existing environment
- installs runtime and build dependencies
- runs `compileall` as a non-GUI sanity check
- builds a one-file `ScaleLogger.exe` into `release\`
- writes `release\SHA256SUMS.txt` in standard single-line release format
- verifies the final one-file executable before hashing it
- writes `release\BUILD_MANIFEST_0.95.txt`
- removes `release\main.build`, `release\main.dist`, and `release\main.onefile-build` after a successful build when cleanup is possible

## 8. One-file build (manual/full 64-bit workflow)

```powershell
build_nuitka_onefile.bat
```

## 9. Standalone build (full 64-bit workflow)

```powershell
build_nuitka_standalone.bat
```

## 10. Generate SHA256 for an existing release artifact

```powershell
generate_sha256.bat path\to\ScaleLogger.exe
```

The helper uses Windows `certutil`, extracts the SHA256 value, and writes `SHA256SUMS.txt` in standard release format:

```text
<sha256>  ScaleLogger.exe
```

## 11. Build manifest details

Use `BUILD_MANIFEST_TEMPLATE.md` for manual builds, or keep the auto-generated `BUILD_MANIFEST_0.95.txt` from `ScaleLogger_build_release.bat`.

The manifest records:

- app name and version
- build date/time (UTC)
- Windows version/build
- Python version
- Nuitka version
- PySide6 version
- pyserial version
- build script used
- output filename
- SHA256 file reference and SHA256 value

## Notes about the packaged application

- The final packaged application does **not** require Python on the target machine.
- One-file builds may still be more fragile on managed Windows systems because antivirus and endpoint tooling can interfere with temporary extraction or file locking.
- Unsigned one-file executables may trigger Defender or SmartScreen detections on some systems.
- Standalone builds may trigger fewer detections than one-file builds on some systems.
- Code signing / SignPath is planned for 1.0, but it is not required for v0.95.
- Relative config example paths such as `logs` and `presets` resolve under the persistent per-user ScaleLogger data directory by design.
- The app uses normal user-home path resolution rather than hardcoded English Windows folder names, so localized Windows installations are supported.

## Runtime usability notes

- The Serial settings tab can scan available ports and open the Test Receive monitor for troubleshooting.
- Baud rate and timeout use editable dropdowns so common presets are easy to pick while custom values remain possible.
- Parity accepts practical values such as `O`, `Odd`, `E`, `Even`, `N`, and `None` and normalizes them internally.


## Windows Defender and one-file builds (local testing)

ScaleLogger's one-file executable extracts internal runtime files when it starts. On a typical Windows system the temporary extraction path looks like:

```text
%LOCALAPPDATA%\Temp\onefile_XXXX\
```

Files such as `main.dll` in that temporary folder are normal and required for one-file execution. The extraction location is managed by the packaging runtime and is not reliably configurable in this project.

Why detections can happen:

- the executable is unsigned
- one-file startup extracts runtime payloads into a temporary folder
- the app uses keyboard injection after parsing scale output
- Defender and other antivirus products may use heuristic or ML-based classification

For local trusted testing:

- place `ScaleLogger.exe` in a stable folder such as `C:\Users\<user>\ScaleLogger\`
- optionally create a shortcut elsewhere

Recommended Defender exclusion steps on Windows 10/11:

1. Open **Windows Security**
2. Go to **Virus & threat protection**
3. Click **Manage settings**
4. Scroll to **Exclusions**
5. Click **Add or remove exclusions**
6. Add:
   - preferably the folder containing `ScaleLogger.exe`
   - not only the EXE file

Important clarification:

- excluding only `ScaleLogger.exe` may not be sufficient
- extracted runtime files such as `main.dll` in the temp folder may still be scanned
- excluding the folder that contains `ScaleLogger.exe` is usually more effective for local testing

Limitations:

- exclusions reduce scanning but do not guarantee zero detections
- do not disable Defender entirely
- this behavior is expected for unsigned one-file applications

Alternative:

- standalone builds do not extract runtime files to a temp folder
- standalone builds may trigger fewer antivirus detections on some systems
