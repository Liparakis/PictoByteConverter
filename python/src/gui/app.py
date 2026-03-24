from __future__ import annotations

import sys

from gui.qt import PYSIDE_AVAILABLE

if PYSIDE_AVAILABLE:
    from gui.qt import QApplication
    from gui.window import PictoByteWindow
else:
    QApplication = None
    PictoByteWindow = None


def main() -> int:
    if not PYSIDE_AVAILABLE:
        raise RuntimeError("PySide6 is required to run the GUI. Install the GUI dependencies first.")
    assert QApplication is not None
    assert PictoByteWindow is not None
    app = QApplication(sys.argv)
    app.setApplicationName("PictoByte Studio")
    app.setOrganizationName("PictoByte")
    window = PictoByteWindow()
    window.show()
    return app.exec()
