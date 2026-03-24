# -*- mode: python ; coding: utf-8 -*-

import platform
from pathlib import Path


repo_root = Path(SPECPATH).parent
python_dir = repo_root / "python"
src_dir = python_dir / "src"
stage_dir = repo_root / "python" / "bin" / "gui-stage"

datas = []
if stage_dir.exists():
    for candidate in stage_dir.iterdir():
        if candidate.suffix.lower() in {".dll", ".so", ".dylib"}:
            datas.append((str(candidate), "."))

hiddenimports = ["pictobyte", "gui", "gui.app", "library", "packaging"]

a = Analysis(
    [str(python_dir / "pictobyte_gui.py")],
    pathex=[str(python_dir), str(src_dir)],
    binaries=[],
    datas=datas,
    hiddenimports=hiddenimports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name="PictoByteStudio",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)

if platform.system() == "Darwin":
    app = BUNDLE(
        exe,
        name="PictoByteStudio.app",
        icon=None,
        bundle_identifier=None,
    )
