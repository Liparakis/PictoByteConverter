from __future__ import annotations

from pathlib import Path

from gui.qt import (
    QLineEdit, QMimeData, Signal, QCheckBox, QPainter, QPaintEvent, QColor, 
    QRect, Qt, Property, QPropertyAnimation, QRectF, QPoint, QSize, QBrush, 
    QPen, QPainterPath, QHBoxLayout, QWidget, QPushButton
)


class DropLineEdit(QLineEdit):
    fileDropped = Signal(str)

    def __init__(self, placeholder: str, *, directory_only: bool = False, bmp_only: bool = False):
        super().__init__()
        self.directory_only = directory_only
        self.bmp_only = bmp_only
        self.setPlaceholderText(placeholder)
        self.setAcceptDrops(True)

    def dragEnterEvent(self, event):  # type: ignore[override]
        if self._extract_path(event.mimeData()):
            event.acceptProposedAction()
            return
        event.ignore()

    def dropEvent(self, event):  # type: ignore[override]
        path = self._extract_path(event.mimeData())
        if path:
            self.setText(path)
            self.fileDropped.emit(path)
            event.acceptProposedAction()
            return
        event.ignore()

    def _extract_path(self, mime_data: QMimeData) -> str | None:
        if not mime_data.hasUrls():
            return None
        for url in mime_data.urls():
            local = url.toLocalFile()
            if not local:
                continue
            candidate = Path(local)
            if self.directory_only and not candidate.is_dir():
                continue
            if not self.directory_only and candidate.is_dir():
                continue
            if self.bmp_only and candidate.suffix.lower() != ".bmp":
                continue
            return str(candidate)
        return None

class ToggleSwitch(QCheckBox):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(40, 20)
        self.setCursor(Qt.PointingHandCursor)
        self._position = 0.0
        self.animation = QPropertyAnimation(self, b"position")
        self.animation.setDuration(150)
        self.stateChanged.connect(self.setup_animation)

    @Property(float)
    def position(self):
        return self._position

    @position.setter
    def position(self, pos):
        self._position = pos
        self.update()

    def setup_animation(self, value):
        self.animation.stop()
        if value:
            self.animation.setEndValue(1.0)
        else:
            self.animation.setEndValue(0.0)
        self.animation.start()

    def hitButton(self, pos: QPoint):
        return self.contentsRect().contains(pos)

    def paintEvent(self, e: QPaintEvent):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        
        status = self.isChecked()
        p.setPen(Qt.NoPen)
        p.setBrush(QColor("#243b35") if status else QColor("#1e2730"))
        
        rect = QRectF(0, 0, self.width(), self.height())
        p.drawRoundedRect(rect, 10, 10)
        
        head = self.height() - 4
        x = 2 + (self.width() - head - 4) * self._position
        p.setBrush(QColor("#7dfba0") if status else QColor("#566b7a"))
        p.drawEllipse(QRectF(x, 2, head, head))
        p.end()

class SpinBoxWidget(QWidget):
    def __init__(self, parent=None, min_val=0, max_val=1024, default_val=0):
        super().__init__(parent)
        self.min_val = min_val
        self.max_val = max_val
        
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        
        self.btn_minus = QPushButton("-")
        self.btn_minus.setObjectName("btnSpinMinus")
        self.btn_minus.setFixedSize(36, 36)
        
        self.input_edit = QLineEdit()
        self.input_edit.setObjectName("spinInput")
        self.input_edit.setAlignment(Qt.AlignCenter)
        self.input_edit.setFixedHeight(36)
        
        self.btn_plus = QPushButton("+")
        self.btn_plus.setObjectName("btnSpinPlus")
        self.btn_plus.setFixedSize(36, 36)
        
        layout.addWidget(self.btn_minus)
        layout.addWidget(self.input_edit)
        layout.addWidget(self.btn_plus)
        
        self.setValue(default_val)
        
        self.btn_minus.clicked.connect(self.decrement)
        self.btn_plus.clicked.connect(self.increment)
        self.input_edit.textChanged.connect(self._validate_text)
        
    def value(self) -> int:
        try:
            return int(self.input_edit.text() or 0)
        except ValueError:
            return self.min_val
            
    def setValue(self, val: int):
        val = max(self.min_val, min(self.max_val, val))
        self.input_edit.setText(str(val))
        
    def decrement(self):
        self.setValue(self.value() - 1)
        
    def increment(self):
        self.setValue(self.value() + 1)
        
    def _validate_text(self, text):
        if not text or text == "-": return
        try:
            val = int(text)
            if val < self.min_val:
                self.input_edit.setText(str(self.min_val))
            elif val > self.max_val:
                self.input_edit.setText(str(self.max_val))
        except ValueError:
            self.input_edit.setText(str(self.min_val))
