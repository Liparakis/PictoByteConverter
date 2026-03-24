from __future__ import annotations

from pathlib import Path
from typing import Callable
import os

from gui.models import DecodeRequest, EncodeRequest
from gui.qt import (
    QFileDialog,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QVBoxLayout,
    QLabel,
    QLineEdit,
    QPushButton,
    QSpinBox,
    QWidget,
    Qt,
)
from gui.widgets import DropLineEdit, SpinBoxWidget



class EncodeTabDelegate:
    def __init__(self, on_start: Callable[[], None]):
        self.input_edit = DropLineEdit("Drop a file here or browse...")
        self.output_edit = QLineEdit()
        self.output_edit.setPlaceholderText("e.g. D:/encoded/project_backup")
        
        max_threads = os.cpu_count() or 4
        
        self.chunk_spin = SpinBoxWidget(min_val=1, max_val=1024, default_val=9)
        self.chunk_spin.setFixedWidth(200)
        
        self.threads_spin = SpinBoxWidget(min_val=0, max_val=max_threads, default_val=2)
        self.threads_spin.setFixedWidth(200)
        
        self.start_button = QPushButton("▶ START ENCODE")
        self.start_button.setObjectName("btnStart")
        self.start_button.clicked.connect(on_start)
        
        self.start_button.clicked.connect(on_start)

    def build(self) -> QWidget:
        widget = QWidget()
        layout = QVBoxLayout(widget)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(12)
        
        title = QLabel("ENCODE SETTINGS")
        title.setObjectName("tabTitle")
        
        title_row = QHBoxLayout()
        title_row.addWidget(title)
        line = QWidget()
        line.setFixedHeight(1)
        line.setStyleSheet("background-color: #202b36;")
        title_row.addWidget(line, 1)
        layout.addLayout(title_row)
        
        form_wrapper = QVBoxLayout()
        form_wrapper.setSpacing(16)
        
        form_wrapper.addWidget(self._form_row("Source File", self._path_row(self.input_edit, self.browse_source)))
        form_wrapper.addWidget(self._form_row("Output Base", self._path_row(self.output_edit, self.browse_output)))
        form_wrapper.addWidget(self._form_row("Chunk Size (MiB)", self.chunk_spin))
        form_wrapper.addWidget(self._form_row("Threads", self.threads_spin))
        
        layout.addLayout(form_wrapper)

        actions = QHBoxLayout()
        line2 = QWidget()
        line2.setFixedHeight(1)
        line2.setStyleSheet("background-color: #202b36;")
        layout.addWidget(line2)
        
        actions.setContentsMargins(0, 15, 0, 0)
        actions.addStretch(1)
        actions.addWidget(self.start_button)
        layout.addLayout(actions)
        return widget
        
    def _form_row(self, label_text: str, field_widget: QWidget) -> QWidget:
        row = QWidget()
        row_layout = QHBoxLayout(row)
        row_layout.setContentsMargins(0, 0, 0, 0)
        row_layout.setSpacing(16)
        lbl = QLabel(label_text)
        lbl.setFixedWidth(120)
        lbl.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        lbl.setStyleSheet("color: #8fa3b3; font-family: Consolas; font-size: 13px;")
        row_layout.addWidget(lbl)
        row_layout.addWidget(field_widget, 1)
        return row

    def browse_source(self) -> None:
        path, _ = QFileDialog.getOpenFileName(None, "Choose file to encode")
        if path:
            self.input_edit.setText(path)

    def browse_output(self) -> None:
        directory = QFileDialog.getExistingDirectory(None, "Choose output directory")
        if not directory:
            return
        suggested = Path(directory) / Path(self.input_edit.text() or "encoded_data").stem
        self.output_edit.setText(str(suggested))

    def _path_row(self, field: QLineEdit, on_browse: Callable[[], None]) -> QWidget:
        wrapper = QWidget()
        layout = QHBoxLayout(wrapper)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(12)
        browse = QPushButton("BROWSE")
        browse.setObjectName("btnBrowse")
        browse.setFixedWidth(100)
        browse.clicked.connect(on_browse)
        layout.addWidget(field, 1)
        layout.addWidget(browse)
        return wrapper

    def read(self) -> EncodeRequest:
        return EncodeRequest(
            input_path=self.input_edit.text(),
            output_base=self.output_edit.text(),
            chunk_size_mb=self.chunk_spin.value(),
            num_threads=self.threads_spin.value(),
        )


class DecodeTabDelegate:
    def __init__(self, on_start: Callable[[], None]):
        import os
        max_threads = os.cpu_count() or 4
        
        self.input_edit = DropLineEdit("Drop any BMP chunk here or browse...", bmp_only=True)
        self.output_edit = DropLineEdit("Drop a folder here or browse...", directory_only=True)
        self.threads_spin = SpinBoxWidget(min_val=0, max_val=max_threads, default_val=2)
        self.threads_spin.setFixedWidth(200)
        
        self.start_button = QPushButton("▶ START DECODE")
        self.start_button.setObjectName("btnStart")
        self.start_button.clicked.connect(on_start)
        
        self.start_button.clicked.connect(on_start)

    def build(self) -> QWidget:
        widget = QWidget()
        layout = QVBoxLayout(widget)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(12)
        
        title = QLabel("DECODE SETTINGS")
        title.setObjectName("tabTitle")
        
        title_row = QHBoxLayout()
        title_row.addWidget(title)
        line = QWidget()
        line.setFixedHeight(1)
        line.setStyleSheet("background-color: #202b36;")
        title_row.addWidget(line, 1)
        layout.addLayout(title_row)
        
        form_wrapper = QVBoxLayout()
        form_wrapper.setSpacing(16)
        
        form_wrapper.addWidget(self._form_row("BMP Chunk", self._path_row(self.input_edit, self.browse_source)))
        form_wrapper.addWidget(self._form_row("Output Directory", self._path_row(self.output_edit, self.browse_output)))
        form_wrapper.addWidget(self._form_row("Threads", self.threads_spin))
        
        layout.addLayout(form_wrapper)

        actions = QHBoxLayout()
        line2 = QWidget()
        line2.setFixedHeight(1)
        line2.setStyleSheet("background-color: #202b36;")
        layout.addWidget(line2)
        
        actions.setContentsMargins(0, 15, 0, 0)
        actions.addStretch(1)
        actions.addWidget(self.start_button)
        layout.addLayout(actions)
        return widget

    def _form_row(self, label_text: str, field_widget: QWidget) -> QWidget:
        row = QWidget()
        row_layout = QHBoxLayout(row)
        row_layout.setContentsMargins(0, 0, 0, 0)
        row_layout.setSpacing(16)
        lbl = QLabel(label_text)
        lbl.setFixedWidth(120)
        lbl.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        lbl.setStyleSheet("color: #8fa3b3; font-family: Consolas; font-size: 13px;")
        row_layout.addWidget(lbl)
        row_layout.addWidget(field_widget, 1)
        return row

    def browse_source(self) -> None:
        path, _ = QFileDialog.getOpenFileName(None, "Choose any BMP chunk", "", "Bitmap Files (*.bmp)")
        if path:
            self.input_edit.setText(path)

    def browse_output(self) -> None:
        directory = QFileDialog.getExistingDirectory(None, "Choose restore directory")
        if directory:
            self.output_edit.setText(directory)

    def _path_row(self, field: QLineEdit, on_browse: Callable[[], None]) -> QWidget:
        wrapper = QWidget()
        layout = QHBoxLayout(wrapper)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(12)
        browse = QPushButton("BROWSE")
        browse.setObjectName("btnBrowse")
        browse.setFixedWidth(100)
        browse.clicked.connect(on_browse)
        layout.addWidget(field, 1)
        layout.addWidget(browse)
        return wrapper

    def read(self) -> DecodeRequest:
        return DecodeRequest(
            input_image_path=self.input_edit.text(),
            output_dir=self.output_edit.text(),
            num_threads=self.threads_spin.value(),
        )
