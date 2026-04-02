from __future__ import annotations

from datetime import datetime, timezone
from importlib import metadata
from pathlib import Path
import platform
import shutil
import sys

from .version import APP_NAME, APP_VERSION

DEFAULT_NOTES = (
    "Unsigned transitional Python/PySide6 build. SHA256 and build manifest improve traceability while native "
    "Win32/C++ implementation is still in progress."
)
ARTIFACT_DIRS = ("main.build", "main.dist", "main.onefile-build")
MIN_RELEASE_EXE_SIZE = 1_000_000


def detect_package_version(distribution_name: str, module_fallback: str | None = None) -> str:
    try:
        return metadata.version(distribution_name)
    except metadata.PackageNotFoundError:
        if module_fallback is not None:
            try:
                module = __import__(module_fallback)
            except ModuleNotFoundError:
                return "unknown"
            return getattr(module, "__version__", "unknown")
        return "unknown"


def utc_build_timestamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def read_sha256sum(checksum_file: Path) -> tuple[str, str]:
    line = checksum_file.read_text(encoding="utf-8").strip()
    parts = line.split()
    if len(parts) < 2:
        raise ValueError("SHA256SUMS.txt must contain '<sha256>  <filename>'.")
    return parts[0], parts[-1]


def build_manifest_text(
    checksum_file: Path,
    output_filename: str,
    build_script: str,
    notes: str = DEFAULT_NOTES,
) -> str:
    sha256_value, _ = read_sha256sum(checksum_file)
    lines = [
        f"App name: {APP_NAME}",
        f"Version: {APP_VERSION}",
        f"Build date/time (UTC): {utc_build_timestamp()}",
        f"Windows version/build: {platform.platform()}",
        f"Python version: {platform.python_version()}",
        f"Nuitka version: {detect_package_version('Nuitka', 'nuitka')}",
        f"PySide6 version: {detect_package_version('PySide6', 'PySide6')}",
        f"pyserial version: {detect_package_version('pyserial', 'serial')}",
        f"Build script: {build_script}",
        f"Output filename: {output_filename}",
        f"SHA256 file: {checksum_file.name}",
        f"SHA256: {sha256_value}",
        f"Notes: {notes}",
    ]
    return "\n".join(lines) + "\n"


def write_manifest_file(manifest_path: Path, checksum_file: Path, output_filename: str, build_script: str) -> Path:
    manifest_path.write_text(build_manifest_text(checksum_file, output_filename, build_script), encoding="utf-8")
    return manifest_path


def verify_release_executable(executable_path: Path, min_size_bytes: int = MIN_RELEASE_EXE_SIZE) -> tuple[bool, str]:
    if not executable_path.exists():
        return False, f"Missing release executable: {executable_path}"
    size = executable_path.stat().st_size
    if size < min_size_bytes:
        return False, f"Release executable is unexpectedly small ({size} bytes)."
    return True, f"Release executable size looks valid ({size} bytes)."


def cleanup_nuitka_artifacts(root: Path) -> tuple[list[str], list[str]]:
    removed: list[str] = []
    failed: list[str] = []
    for dirname in ARTIFACT_DIRS:
        path = root / dirname
        if not path.exists():
            continue
        try:
            shutil.rmtree(path)
            removed.append(path.name)
        except OSError:
            failed.append(path.name)
    return removed, failed


def main(argv: list[str] | None = None) -> int:
    args = list(argv or sys.argv[1:])
    if not args:
        print("Usage: python -m app.release_support manifest <manifest_path> <checksum_file> <output_filename> <build_script>")
        print("   or: python -m app.release_support verify <executable_path>")
        print("   or: python -m app.release_support cleanup [root]")
        return 1

    command = args.pop(0)
    if command == "manifest" and len(args) == 4:
        manifest_path = Path(args[0])
        checksum_file = Path(args[1])
        write_manifest_file(manifest_path, checksum_file, args[2], args[3])
        print(manifest_path)
        return 0
    if command == "cleanup" and len(args) <= 1:
        root = Path(args[0]) if args else Path.cwd()
        removed, failed = cleanup_nuitka_artifacts(root)
        for name in removed:
            print(f"removed:{name}")
        for name in failed:
            print(f"failed:{name}")
        return 0 if not failed else 2
    if command == "verify" and len(args) == 1:
        ok, message = verify_release_executable(Path(args[0]))
        print(message)
        return 0 if ok else 2

    print("Invalid release_support command.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
