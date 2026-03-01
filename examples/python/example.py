import hashlib
import os
import shutil
import tempfile
import time
from pathlib import Path

# pictobyte.py lives in <repo>/python/, two levels up from examples/python/
import sys
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "python"))

from pictobyte import PictoByteConverter

# ─── Configuration ────────────────────────────────────────────────────────────
TEST_SIZE_MB     = 1000   # size of the synthetic test file
CHUNK_SIZE_MB    = 9    # target BMP chunk size
NUM_THREADS      = 0    # 0 = auto-detect

# ─── Helpers ─────────────────────────────────────────────────────────────────

def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def hr_size(n: int) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return f"{n:.1f} {unit}"
        n /= 1024
    return f"{n:.1f} TB"


# ─── Main ─────────────────────────────────────────────────────────────────────

def main():
    tmp_dir = Path(tempfile.mkdtemp(prefix="pictobyte_test_"))
    encode_dir = tmp_dir / "encoded"
    decode_dir = tmp_dir / "decoded"
    encode_dir.mkdir()
    decode_dir.mkdir()

    src_file = tmp_dir / "test_input.bin"
    print(f"[*] Creating {TEST_SIZE_MB} MB test file: {src_file}")
    with open(src_file, "wb") as f:
        chunk = os.urandom(1 << 20)  # 1 MB at a time
        for _ in range(TEST_SIZE_MB):
            f.write(chunk)

    original_hash = sha256_file(src_file)
    print(f"[*] SHA-256 (original): {original_hash}")

    pb = PictoByteConverter(verbose=True)
    print(f"[*] Library version: {pb.version}")

    # ── Encode ──────────────────────────────────────────────────────────────
    output_base = str(encode_dir / "test_input")
    print(f"\n[*] Encoding -> {output_base}_Xof<N>.bmp  (chunk={CHUNK_SIZE_MB} MB)")
    t0 = time.perf_counter()
    pb.encode(src_file, output_base,
              chunk_size_mb=CHUNK_SIZE_MB,
              num_threads=NUM_THREADS)
    encode_time = time.perf_counter() - t0

    bmp_files = sorted(encode_dir.glob("*.bmp"))
    total_bmp_size = sum(f.stat().st_size for f in bmp_files)
    print(f"[+] Encoded {len(bmp_files)} chunks in {encode_time:.2f}s "
          f"({hr_size(total_bmp_size)} total BMP)")

    # ── Decode ──────────────────────────────────────────────────────────────
    print(f"\n[*] Decoding -> {decode_dir}")
    t0 = time.perf_counter()
    pb.decode(str(bmp_files[0]), str(decode_dir), num_threads=NUM_THREADS)
    decode_time = time.perf_counter() - t0
    print(f"[+] Decoded in {decode_time:.2f}s")

    # ── Verify ──────────────────────────────────────────────────────────────
    restored = decode_dir / src_file.name
    restored_hash = sha256_file(restored)
    print(f"\n[*] SHA-256 (restored): {restored_hash}")

    if original_hash == restored_hash:
        print("[✓] Hashes MATCH — round-trip successful!")
    else:
        print("[✗] Hash MISMATCH — something went wrong!")
        sys.exit(1)

    # ── Cleanup ─────────────────────────────────────────────────────────────
    shutil.rmtree(tmp_dir)
    print("[*] Temp files cleaned up.")


if __name__ == "__main__":
    main()