from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from packaging import build_pyinstaller_bundle


if __name__ == "__main__":
    raise SystemExit(build_pyinstaller_bundle())
