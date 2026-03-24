from __future__ import annotations

import re
from pathlib import Path

from gui.formatting import format_bytes


def build_encode_summary(output_base: str) -> str:
    base = Path(output_base)
    directory = base.parent if base.parent != Path("") else Path.cwd()
    pattern = re.compile(rf"^{re.escape(base.name)}_\d+of\d+\.bmp$", re.IGNORECASE)
    files = sorted(path for path in directory.glob("*.bmp") if pattern.match(path.name))
    if not files:
        return "Encoding finished, but no BMP chunks were discovered next to the chosen output base."
    total_size = sum(path.stat().st_size for path in files)
    return (
        f"Created {len(files)} BMP chunks in {directory}.\n"
        f"Total BMP size: {format_bytes(total_size)}\n"
        f"First chunk: {files[0].name}"
    )


def build_decode_summary(output_dir: str) -> str:
    directory = Path(output_dir)
    files = sorted(
        (path for path in directory.iterdir() if path.is_file()),
        key=lambda path: path.stat().st_mtime,
        reverse=True,
    )
    if not files:
        return "Decode finished, but no restored file was found in the output directory."
    restored = files[0]
    return f"Restored file: {restored.name}\nSize: {format_bytes(restored.stat().st_size)}\nLocation: {restored}"
