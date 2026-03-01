"""
pictobyte.py — Python wrapper for the PictoByteConverter shared library.

Usage:
    from pictobyte import PictoByteConverter

    pb = PictoByteConverter()          # auto-discovers the .dll / .so
    pb.encode("D:/bigfile.iso", "D:/out/bigfile", chunk_size_mb=9)
    pb.decode("D:/out/bigfile_1of40.bmp", "D:/restored/")

The constructor accepts an optional `lib_path` argument to point directly at
the shared library file if auto-discovery fails.
"""

import ctypes
import os
import sys
import platform
from pathlib import Path

# On Windows, we may need to add the compiler's bin directory to the DLL search path
# if we're running from a development environment (like CLion/MinGW).
if platform.system() == "Windows" and sys.version_info >= (3, 8):
    # Try to find common MinGW paths from CLion
    _clion_mingw = Path("C:/Program Files/JetBrains/CLion 2025.2.1/bin/mingw/bin")
    if _clion_mingw.exists():
        os.add_dll_directory(str(_clion_mingw))
    
    # Also add the directory containing the DLL itself
    _bin_dirs = [
        Path(__file__).parent,
        Path(__file__).parent.parent / "cmake-build-release" / "bin",
        Path(__file__).parent.parent / "cmake-build-debug" / "bin",
        Path(__file__).parent.parent / "build" / "bin",
    ]
    for _d in _bin_dirs:
        if _d.exists():
            os.add_dll_directory(str(_d))


# ─── Library discovery ────────────────────────────────────────────────────────

def _find_library() -> Path:
    """
    Search for the compiled shared library in order of preference.
    """
    system = platform.system()
    if system == "Windows":
        names = ["pictobyte.dll", "libpictobyte.dll"]
    elif system == "Darwin":
        names = ["libpictobyte.dylib"]
    else:
        names = ["libpictobyte.so"]

    candidates = []
    search_dirs = [
        Path(__file__).parent,
        Path(__file__).parent.parent / "build" / "bin",
        Path(__file__).parent.parent / "build",
        Path(__file__).parent.parent / "cmake-build-release" / "bin",
        Path(__file__).parent.parent / "cmake-build-release",
        Path(__file__).parent.parent / "cmake-build-debug" / "bin",
        Path(__file__).parent.parent / "cmake-build-debug",
    ]

    for d in search_dirs:
        for name in names:
            candidates.append(d / name)

    for p in candidates:
        if p.exists():
            return p

    # Fall back to letting the OS find the first name
    return Path(names[0])


# ─── ctypes binding ───────────────────────────────────────────────────────────

# Callback type: void (*)(const char* msg, void* user_data)
_LogCallbackType = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_void_p)


class _Lib:
    """Low-level ctypes wrapper. One instance is shared per process."""

    def __init__(self, lib_path: Path):
        self._dll = ctypes.CDLL(str(lib_path))
        self._setup_prototypes()

    def _setup_prototypes(self):
        d = self._dll

        # int pb_encode(const char*, const char*, unsigned, unsigned,
        #               pb_log_callback_t, void*)
        d.pb_encode.restype  = ctypes.c_int
        d.pb_encode.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p,
            ctypes.c_uint,   ctypes.c_uint,
            _LogCallbackType, ctypes.c_void_p,
        ]

        # int pb_decode(const char*, const char*, unsigned,
        #               pb_log_callback_t, void*)
        d.pb_decode.restype  = ctypes.c_int
        d.pb_decode.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p,
            ctypes.c_uint,
            _LogCallbackType, ctypes.c_void_p,
        ]

        # const char* pb_last_error(void)
        d.pb_last_error.restype  = ctypes.c_char_p
        d.pb_last_error.argtypes = []

        # const char* pb_version(void)
        d.pb_version.restype  = ctypes.c_char_p
        d.pb_version.argtypes = []


# ─── Public API ───────────────────────────────────────────────────────────────

class PictoByteError(RuntimeError):
    """Raised when the C library returns a non-zero error code."""


class PictoByteConverter:
    """
    High-level Python interface to the PictoByteConverter library.

    Parameters
    ----------
    lib_path : str or Path, optional
        Explicit path to pictobyte.dll / libpictobyte.so.
        If omitted, the library is auto-discovered.
    verbose : bool
        If True, log messages from the C++ library are printed to stdout.
    log_callback : callable, optional
        Called with each log line as a plain str.  Overrides `verbose`.
    """

    def __init__(self, lib_path=None, *, verbose: bool = False, log_callback=None):
        path = Path(lib_path) if lib_path else _find_library()
        self._lib = _Lib(path)
        self._user_log = log_callback
        self._verbose  = verbose

    # ── Helpers ──────────────────────────────────────────────────────────────

    @property
    def version(self) -> str:
        return self._lib._dll.pb_version().decode()

    def _make_callback(self):
        """Return a ctypes callback that routes messages to the user's handler."""
        user_log = self._user_log
        verbose  = self._verbose

        def _cb(msg_bytes: bytes, _user_data):
            msg = msg_bytes.decode(errors="replace") if msg_bytes else ""
            if user_log:
                user_log(msg)
            elif verbose:
                print("[pictobyte]", msg)

        # Keep a reference on self so it isn't GC'd while the C lib holds it
        self._cb_ref = _LogCallbackType(_cb)
        return self._cb_ref

    @staticmethod
    def _enc(s) -> bytes:
        return os.fsencode(str(s))

    def _check(self, rc: int):
        if rc != 0:
            err = self._lib._dll.pb_last_error()
            raise PictoByteError(err.decode(errors="replace") if err else "Unknown error")

    # ── Public methods ───────────────────────────────────────────────────────

    def encode(self,
               input_path,
               output_base,
               chunk_size_mb: int = 9,
               num_threads: int = 0) -> None:
        """
        Convert *input_path* to one or more BMP images.

        Parameters
        ----------
        input_path : str or Path
            Source file to encode.
        output_base : str or Path
            Base path for output images, e.g. ``"D:/out/myfile"``.
            Output files will be named ``myfile_1of<N>.bmp``, etc.
        chunk_size_mb : int
            Target BMP file size per chunk in MiB (default 9).
        num_threads : int
            Number of worker threads (0 = auto-detect).
        """
        cb = self._make_callback()
        rc = self._lib._dll.pb_encode(
            self._enc(input_path),
            self._enc(output_base),
            ctypes.c_uint(chunk_size_mb),
            ctypes.c_uint(num_threads),
            cb,
            None,
        )
        self._check(rc)

    def decode(self,
               input_image_path,
               output_dir,
               num_threads: int = 0) -> None:
        """
        Reconstruct the original file from a set of BMP chunk images.

        Parameters
        ----------
        input_image_path : str or Path
            Path to *any one* of the BMP chunk files.
        output_dir : str or Path
            Directory where the reconstructed file will be written.
        num_threads : int
            Number of worker threads (0 = auto-detect).
        """
        cb = self._make_callback()
        rc = self._lib._dll.pb_decode(
            self._enc(input_image_path),
            self._enc(output_dir),
            ctypes.c_uint(num_threads),
            cb,
            None,
        )
        self._check(rc)
