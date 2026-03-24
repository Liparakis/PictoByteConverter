from __future__ import annotations

import shutil
import subprocess
import sys
import platform
from pathlib import Path

from library import iter_library_candidates, resolve_library_path


def _shared_library_suffixes() -> set[str]:
    system = platform.system()
    if system == "Windows":
        return {".dll"}
    if system == "Darwin":
        return {".dylib"}
    return {".so"}


def stage_native_library(stage_dir: Path) -> Path:
    stage_dir.mkdir(parents=True, exist_ok=True)
    candidates = iter_library_candidates()
    library_path = resolve_library_path()
    if not library_path.exists():
        searched = "\n".join(str(candidate) for candidate in candidates)
        raise FileNotFoundError(
            "Could not find the PictoByte shared library.\n"
            "Build the project first so pictobyte.dll exists.\n"
            f"Searched:\n{searched}"
        )
    target_path = stage_dir / library_path.name
    shutil.copy2(library_path, target_path)
    return target_path


def packaged_output_hint(output_dir: Path) -> str:
    system = platform.system()
    if system == "Windows":
        return str(output_dir / "PictoByteStudio.exe")
    if system == "Darwin":
        return str(output_dir / "PictoByteStudio.app")
    return str(output_dir / "PictoByteStudio")


def build_pyinstaller_bundle() -> int:
    repo_root = Path(__file__).resolve().parents[2]
    python_bin_dir = repo_root / "python" / "bin"
    stage_dir = python_bin_dir / "gui-stage"
    dist_dir = python_bin_dir
    work_dir = python_bin_dir / "pyinstaller-work"
    staged_library = stage_native_library(stage_dir)
    print(f"Staged native library: {staged_library}")
    print(f"Expected packaged output: {packaged_output_hint(dist_dir)}")

    command = [
        sys.executable,
        "-m",
        "PyInstaller",
        str(repo_root / "python" / "pictobyte_gui.spec"),
        "--noconfirm",
        "--distpath",
        str(dist_dir),
        "--workpath",
        str(work_dir),
    ]
    return subprocess.call(command, cwd=repo_root)
