from __future__ import annotations

from gui.models import DecodeRequest, EncodeRequest, JobResult
from gui.qt import QObject, Signal
from gui.summaries import build_decode_summary, build_encode_summary
from library import PictoByteConverter, PictoByteError


class ConversionWorker(QObject):
    log = Signal(str)
    finished = Signal(object)
    failed = Signal(str)

    def __init__(self, mode: str, payload: EncodeRequest | DecodeRequest, lib_path: str | None, verbose: bool = False):
        super().__init__()
        self.mode = mode
        self.payload = payload
        self.lib_path = lib_path or None
        self.verbose = verbose
        self._cancelled = False

    def cancel(self) -> None:
        self._cancelled = True
        self.log.emit("Cancel requested. The native library call cannot be interrupted, so the result will be ignored if it finishes later.")

    def run(self) -> None:
        try:
            converter = PictoByteConverter(lib_path=self.lib_path, verbose=self.verbose, log_callback=self._emit_log)
            if self.mode == "encode":
                payload = self.payload
                assert isinstance(payload, EncodeRequest)
                converter.encode(
                    payload.input_path,
                    payload.output_base,
                    chunk_size_mb=payload.chunk_size_mb,
                    num_threads=payload.num_threads,
                )
                summary = build_encode_summary(payload.output_base)
            else:
                payload = self.payload
                assert isinstance(payload, DecodeRequest)
                converter.decode(
                    payload.input_image_path,
                    payload.output_dir,
                    num_threads=payload.num_threads,
                )
                summary = build_decode_summary(payload.output_dir)

            if self._cancelled:
                self.finished.emit(JobResult(self.mode, "Job finished after cancellation request. Output may still have been written by the native library."))
                return
            self.finished.emit(JobResult(self.mode, summary))
        except PictoByteError as exc:
            self.failed.emit(str(exc))
        except Exception as exc:
            self.failed.emit(str(exc))

    def _emit_log(self, message: str) -> None:
        if not self._cancelled:
            self.log.emit(message)
