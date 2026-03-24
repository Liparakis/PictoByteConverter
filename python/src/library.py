from __future__ import annotations

import ctypes
import os
import platform
import sys
from pathlib import Path
from typing import Iterable


_CLION_MINGW_DIR = Path("C:/Program Files/JetBrains/CLion 2025.2.1/bin/mingw/bin")
_LOG_CALLBACK_TYPE = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_void_p)


def _library_names() -> list[str]:
    system = platform.system()
    if system == "Windows":
        return ["pictobyte.dll", "libpictobyte.dll"]
    if system == "Darwin":
        return ["libpictobyte.dylib"]
    return ["libpictobyte.so"]


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _default_search_dirs() -> list[Path]:
    repo_root = _repo_root()
    package_dir = Path(__file__).resolve().parent
    python_dir = repo_root / "python"
    search_dirs = [
        package_dir,
        package_dir.parent,
        python_dir,
        python_dir / "bin",
        Path.cwd(),
        repo_root / "build" / "bin",
        repo_root / "build",
        repo_root / "cmake-build-release" / "bin",
        repo_root / "cmake-build-release",
        repo_root / "cmake-build-debug" / "bin",
        repo_root / "cmake-build-debug",
    ]

    if getattr(sys, "frozen", False):
        exe_dir = Path(sys.executable).resolve().parent
        search_dirs.extend([exe_dir, exe_dir / "_internal"])

    meipass = getattr(sys, "_MEIPASS", None)
    if meipass:
        bundle_dir = Path(meipass)
        search_dirs.extend([bundle_dir, bundle_dir / "_internal"])

    return search_dirs


def iter_library_candidates(search_dirs: Iterable[Path] | None = None) -> list[Path]:
    directories = list(search_dirs) if search_dirs is not None else _default_search_dirs()
    seen: set[Path] = set()
    candidates: list[Path] = []
    for directory in directories:
        resolved_dir = Path(directory)
        if resolved_dir in seen:
            continue
        seen.add(resolved_dir)
        for name in _library_names():
            candidates.append(resolved_dir / name)
    return candidates


def configure_dll_search(extra_dirs: Iterable[Path] | None = None) -> None:
    if platform.system() != "Windows" or sys.version_info < (3, 8):
        return

    search_dirs = _default_search_dirs()
    if extra_dirs is not None:
        search_dirs.extend(Path(path) for path in extra_dirs)
    if _CLION_MINGW_DIR.exists():
        search_dirs.append(_CLION_MINGW_DIR)

    seen: set[str] = set()
    for directory in search_dirs:
        if not directory.exists():
            continue
        directory_str = str(directory.resolve())
        if directory_str in seen:
            continue
        seen.add(directory_str)
        os.add_dll_directory(directory_str)


def resolve_library_path(
    lib_path: str | os.PathLike[str] | None = None,
    *,
    search_dirs: Iterable[Path] | None = None,
) -> Path:
    if lib_path:
        explicit_path = Path(lib_path)
        return explicit_path.resolve() if explicit_path.exists() else explicit_path

    for candidate in iter_library_candidates(search_dirs):
        if candidate.exists():
            return candidate.resolve()

    return Path(_library_names()[0])


configure_dll_search()


class _NativeLibrary:
    def __init__(self, lib_path: Path):
        self._dll = ctypes.CDLL(str(lib_path))
        self._setup_prototypes()

    def _setup_prototypes(self) -> None:
        dll = self._dll
        dll.pb_encode.restype = ctypes.c_int
        dll.pb_encode.argtypes = [
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_uint,
            ctypes.c_uint,
            _LOG_CALLBACK_TYPE,
            ctypes.c_void_p,
        ]
        dll.pb_decode.restype = ctypes.c_int
        dll.pb_decode.argtypes = [
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_uint,
            _LOG_CALLBACK_TYPE,
            ctypes.c_void_p,
        ]
        dll.pb_last_error.restype = ctypes.c_char_p
        dll.pb_last_error.argtypes = []
        dll.pb_version.restype = ctypes.c_char_p
        dll.pb_version.argtypes = []


class PictoByteError(RuntimeError):
    """Raised when the native library returns a non-zero error code."""


class PictoByteConverter:
    def __init__(self, lib_path=None, *, verbose: bool = False, log_callback=None):
        path = resolve_library_path(lib_path)
        configure_dll_search([path.parent])
        self._lib = _NativeLibrary(path)
        self._user_log = log_callback
        self._verbose = verbose
        self._callback_ref = None

    @property
    def version(self) -> str:
        return self._lib._dll.pb_version().decode()

    def _make_callback(self):
        user_log = self._user_log
        verbose = self._verbose

        def _callback(message_bytes: bytes, _user_data):
            message = message_bytes.decode(errors="replace") if message_bytes else ""
            if user_log:
                user_log(message)
            elif verbose:
                print("[pictobyte]", message)

        self._callback_ref = _LOG_CALLBACK_TYPE(_callback)
        return self._callback_ref

    @staticmethod
    def _encode_path(value) -> bytes:
        return os.fsencode(str(value))

    def _check(self, return_code: int) -> None:
        if return_code != 0:
            error = self._lib._dll.pb_last_error()
            raise PictoByteError(error.decode(errors="replace") if error else "Unknown error")

    def encode(self, input_path, output_base, chunk_size_mb: int = 9, num_threads: int = 0) -> None:
        callback = self._make_callback()
        return_code = self._lib._dll.pb_encode(
            self._encode_path(input_path),
            self._encode_path(output_base),
            ctypes.c_uint(chunk_size_mb),
            ctypes.c_uint(num_threads),
            callback,
            None,
        )
        self._check(return_code)

    def decode(self, input_image_path, output_dir, num_threads: int = 0) -> None:
        callback = self._make_callback()
        return_code = self._lib._dll.pb_decode(
            self._encode_path(input_image_path),
            self._encode_path(output_dir),
            ctypes.c_uint(num_threads),
            callback,
            None,
        )
        self._check(return_code)
