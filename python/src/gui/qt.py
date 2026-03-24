from __future__ import annotations

PYSIDE_AVAILABLE = False

try:
    from PySide6.QtCore import QMimeData, QObject, QSettings, Qt, QThread, Signal, Property, QPropertyAnimation, QRect, QRectF, QPoint, QSize
    from PySide6.QtGui import QColor, QFont, QPalette, QPainter, QPaintEvent, QBrush, QPen, QPainterPath, QPixmap
    from PySide6.QtWidgets import (
        QApplication,
        QCheckBox,
        QFileDialog,
        QFormLayout,
        QFrame,
        QGridLayout,
        QGroupBox,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QMainWindow,
        QMessageBox,
        QPushButton,
        QPlainTextEdit,
        QSpinBox,
        QStatusBar,
        QTabWidget,
        QVBoxLayout,
        QWidget,
    )

    PYSIDE_AVAILABLE = True
except ImportError:
    pass
