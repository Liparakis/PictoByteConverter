from __future__ import annotations

import datetime as dt

from gui.delegates import DecodeTabDelegate, EncodeTabDelegate
from gui.models import DecodeRequest, EncodeRequest, JobResult
from gui.qt import (
    QApplication,
    QColor,
    QFont,
    QFrame,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPainter,
    QPalette,
    QPlainTextEdit,
    QSettings,
    QStatusBar,
    QTabWidget,
    QThread,
    QVBoxLayout,
    QWidget,
    Qt,
)
from gui.widgets import ToggleSwitch
from gui.validation import validate_decode_request, validate_encode_request
from gui.workers import ConversionWorker
from library import resolve_library_path


class PictoByteWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.settings = QSettings("PictoByte", "PictoByteGUI")
        self.worker_thread: QThread | None = None
        self.worker: ConversionWorker | None = None
        self.encode_delegate = EncodeTabDelegate(self.start_encode)
        self.decode_delegate = DecodeTabDelegate(self.start_decode)
        self._build_ui()
        self.load_settings()
        self._on_verbose_toggled(self.verbose_toggle.isChecked())

    def paintEvent(self, e) -> None:
        p = QPainter(self)
        p.fillRect(self.rect(), QColor("#090e13"))
        
        p.setPen(QColor("#111a22"))
        for i in range(0, self.width(), 40):
            p.drawLine(i, 0, i, self.height())
        for i in range(0, self.height(), 40):
            p.drawLine(0, i, self.width(), i)
            
    def _build_ui(self) -> None:
        self.setWindowTitle("PictoByte Studio")

        root = QWidget()
        layout = QVBoxLayout(root)
        layout.setSizeConstraint(QVBoxLayout.SizeConstraint.SetFixedSize)
        layout.setContentsMargins(24, 24, 24, 16)
        layout.setSpacing(12)
        
        layout.addWidget(self._build_hero())

        self.tabs = QTabWidget()
        self.tabs.setObjectName("mainTabs")
        self.tabs.addTab(self.encode_delegate.build(), "Encode")
        self.tabs.addTab(self.decode_delegate.build(), "Decode")
        layout.addWidget(self.tabs)
        
        self.log_view = QPlainTextEdit()
        self.log_view.setObjectName("logView")
        self.log_view.setReadOnly(True)
        self.log_view.setFixedHeight(160)
        self.log_view.setFont(QFont("Consolas", 11))
        self.log_view.setVisible(self.verbose_toggle.isChecked())
        layout.addWidget(self.log_view)

        self.banner = QLabel()
        self.banner.hide()
        self.summary_label = QLabel()
        self.summary_label.hide()

        self.setCentralWidget(root)
        self._apply_palette()

    def _build_hero(self) -> QWidget:
        hero = QWidget()
        hero_layout = QHBoxLayout(hero)
        hero_layout.setContentsMargins(0, 0, 0, 10)
        hero_layout.setSpacing(16)
        
        left_col = QVBoxLayout()
        left_col.setSpacing(4)
        
        title = QLabel("PictoByte")
        title.setObjectName("heroTitle")
        
        subtitle = QLabel("Encode binary files into BMP chunks and restore\nthem from any segment.")
        subtitle.setObjectName("heroSubtitle")
        
        left_col.addWidget(title)
        left_col.addWidget(subtitle)
        hero_layout.addLayout(left_col)
        
        hero_layout.addStretch(1)
        
        toggle_col = QVBoxLayout()
        toggle_row = QHBoxLayout()
        self.verbose_toggle = ToggleSwitch()
        self.verbose_toggle.setChecked(False)
        self.verbose_toggle.toggled.connect(self._on_verbose_toggled)
        verbose_label = QLabel("verbose")
        verbose_label.setStyleSheet("color: #8fa3b3; font-family: Consolas; font-size: 13px;")
        toggle_row.addWidget(self.verbose_toggle)
        toggle_row.addWidget(verbose_label)
        toggle_col.addLayout(toggle_row)
        toggle_col.setAlignment(Qt.AlignTop | Qt.AlignRight)
        hero_layout.addLayout(toggle_col)
        
        return hero

    def _on_verbose_toggled(self, checked: bool) -> None:
        self.setMinimumSize(0, 0)
        self.setMaximumSize(16777215, 16777215)
        self.log_view.setVisible(checked)
        self.centralWidget().layout().invalidate()
        
        optimal_size = self.centralWidget().sizeHint()
        # Add 40px vertical buffer to prevent QSS font cropping on the bottom buttons
        self.setFixedSize(optimal_size.width(), optimal_size.height() + 40)

    def _apply_palette(self) -> None:
        app = QApplication.instance()
        assert app is not None
        palette = QPalette()
        palette.setColor(QPalette.Window, QColor("#090e13"))
        palette.setColor(QPalette.WindowText, QColor("#f4f7fb"))
        palette.setColor(QPalette.Base, QColor("#111a22"))
        palette.setColor(QPalette.Text, QColor("#f4f7fb"))
        palette.setColor(QPalette.Button, QColor("#163145"))
        palette.setColor(QPalette.ButtonText, QColor("#f4f7fb"))
        palette.setColor(QPalette.Highlight, QColor("#33c5a1"))
        app.setPalette(palette)
        
        self.setStyleSheet(
            """
            QWidget { 
                color: #f4f7fb; 
                font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; 
                font-size: 14px; 
            }
            QMainWindow, QWidget[window="true"] { background: transparent; }
            
            #heroVersion { color: #7dfba0; }
            #heroTitle { font-size: 38px; font-weight: 800; font-family: "Segoe UI Black", Impact, sans-serif; letter-spacing: -1px; }
            #heroSubtitle { color: #566b7a; font-family: Consolas; font-size: 13px; line-height: 1.2; }
            
            #libPill {
                background-color: #12161b; border: 1px solid #202b36; border-radius: 12px;
            }
            #libStatusLabel { font-family: Consolas; font-size: 12px; color: #8fa3b3; }
            
            QTabWidget::pane {
                border: 1px solid #202b36; border-radius: 16px; background: #12161b;
            }
            
            QTabBar::tab {
                background: #12181d; border: 1px solid #202b36; 
                color: #566b7a; padding: 12px 30px; margin-right: 8px; margin-bottom: 24px;
                border-radius: 8px; font-weight: 700; font-size: 14px;
            }
            QTabBar::tab:selected {
                background: #1f272e; color: #ffffff;
            }
            
            #tabTitle {
                color: #7dfba0; font-family: Consolas; font-weight: 700; letter-spacing: 1px; font-size: 12px;
            }
            
            QLineEdit, QPlainTextEdit {
                border: 1px solid #202b36; border-radius: 8px; background: #161c22; 
                padding: 10px 14px; color: #c0cdd6; font-family: Consolas; font-size: 13px;
            }
            QLineEdit:focus, QPlainTextEdit:focus {
                border: 1px solid #33c5a1;
            }
            
            #logView {
                background: #12161b; color: #7dfba0; border-radius: 12px;
                padding: 16px; min-height: 80px; max-height: 100px;
            }
            
            QPushButton {
                border: 1px solid #202b36; border-radius: 8px; background: #1f272e; 
                padding: 10px 16px; font-weight: 700; color: #8fa3b3; letter-spacing: 1px;
            }
            QPushButton:hover { background: #28323a; color: #ffffff; }
            QPushButton:pressed { background: #181f24; border: 1px solid #4a5c6b; }
            QPushButton:disabled { background: #13171c; color: #364552; border-color: #1a2228; }
            
            #btnCancel { color: #ff768a; }
            #btnCancel:hover { background: #3d2228; color: #ff91a1; border-color: #63323c; }
            
            #btnStart { background: #88ffcc; color: #000000; font-weight: 800; border: none; }
            #btnStart:hover { background: #a8ffd9; }
            #btnStart:pressed { background: #6ae5b3; }
            
            #btnSpinMinus, #btnSpinPlus {
                background: #1f272e; border: 1px solid #202b36; border-radius: 0px; padding: 0px; font-size: 18px; color: #566b7a;
            }
            #btnSpinMinus { border-top-left-radius: 8px; border-bottom-left-radius: 8px; }
            #btnSpinPlus { border-top-right-radius: 8px; border-bottom-right-radius: 8px; border-left: none; }
            #spinInput { border-radius: 0px; border-left: none; border-right: none; }
            """
        )

    def start_encode(self) -> None:
        self.log_view.clear()
        draft = self.encode_delegate.read()
        try:
            payload = validate_encode_request(draft.input_path, draft.output_base, draft.chunk_size_mb, draft.num_threads)
        except ValueError as exc:
            self.show_error(str(exc))
            return
        self.save_settings()
        self.launch_worker("encode", payload)

    def start_decode(self) -> None:
        self.log_view.clear()
        draft = self.decode_delegate.read()
        try:
            payload = validate_decode_request(draft.input_image_path, draft.output_dir, draft.num_threads)
        except ValueError as exc:
            self.show_error(str(exc))
            return
        self.save_settings()
        self.launch_worker("decode", payload)

    def launch_worker(self, mode: str, payload: EncodeRequest | DecodeRequest) -> None:
        if self.worker_thread is not None:
            self.show_error("A conversion is already running.")
            return
        self.worker_thread = QThread(self)
        self.worker = ConversionWorker(mode, payload, None, verbose=self.verbose_toggle.isChecked())
        self.worker.moveToThread(self.worker_thread)
        self.worker_thread.started.connect(self.worker.run)
        self.worker.log.connect(self.append_log)
        self.worker.finished.connect(self.handle_success)
        self.worker.failed.connect(self.handle_failure)
        self.worker.finished.connect(self.worker_thread.quit)
        self.worker.failed.connect(self.worker_thread.quit)
        self.worker_thread.finished.connect(self.cleanup_worker)
        self.set_running_state(True, mode)
        self.append_log(f"Started {mode} job at {dt.datetime.now().strftime('%H:%M:%S')}")
        self.worker_thread.start()

    def handle_success(self, result: JobResult) -> None:
        if self.verbose_toggle.isChecked():
            self.append_log(f"> {result.summary}")
        self.append_log(f"> {result.mode.title()} completed successfully.")

    def handle_failure(self, message: str) -> None:
        self.append_log(f"> ERROR: {message}")

    def cleanup_worker(self) -> None:
        self.set_running_state(False, None)
        if self.worker is not None:
            self.worker.deleteLater()
        if self.worker_thread is not None:
            self.worker_thread.deleteLater()
        self.worker = None
        self.worker_thread = None

    def set_running_state(self, running: bool, mode: str | None) -> None:
        self.encode_delegate.start_button.setEnabled(not running)
        self.decode_delegate.start_button.setEnabled(not running)
        self.tabs.setEnabled(not running)

    def append_log(self, message: str) -> None:
        if not self.verbose_toggle.isChecked():
            return
        self.log_view.appendPlainText(message)

    def show_error(self, message: str) -> None:
        QMessageBox.critical(self, "PictoByte Studio", message)

    def closeEvent(self, event) -> None:  # type: ignore[override]
        self.save_settings()
        super().closeEvent(event)

    def load_settings(self) -> None:
        self.encode_delegate.input_edit.setText(self.settings.value("encode/input_path", "", type=str))
        self.encode_delegate.output_edit.setText(self.settings.value("encode/output_base", "", type=str))
        self.encode_delegate.chunk_spin.setValue(self.settings.value("encode/chunk_size_mb", 9, type=int))
        self.encode_delegate.threads_spin.setValue(self.settings.value("encode/threads", 0, type=int))
        self.decode_delegate.input_edit.setText(self.settings.value("decode/input_image_path", "", type=str))
        self.decode_delegate.output_edit.setText(self.settings.value("decode/output_dir", "", type=str))
        self.decode_delegate.threads_spin.setValue(self.settings.value("decode/threads", 0, type=int))

    def save_settings(self) -> None:
        self.settings.setValue("encode/input_path", self.encode_delegate.input_edit.text().strip())
        self.settings.setValue("encode/output_base", self.encode_delegate.output_edit.text().strip())
        self.settings.setValue("encode/chunk_size_mb", self.encode_delegate.chunk_spin.value())
        self.settings.setValue("encode/threads", self.encode_delegate.threads_spin.value())
        self.settings.setValue("decode/input_image_path", self.decode_delegate.input_edit.text().strip())
        self.settings.setValue("decode/output_dir", self.decode_delegate.output_edit.text().strip())
        self.settings.setValue("decode/threads", self.decode_delegate.threads_spin.value())
