from __future__ import annotations

from pathlib import Path

from gui.models import DecodeRequest, EncodeRequest


def validate_encode_request(
    input_path: str,
    output_base: str,
    chunk_size_mb: int,
    num_threads: int,
) -> EncodeRequest:
    source = Path(input_path).expanduser()
    if not input_path.strip():
        raise ValueError("Choose a source file to encode.")
    if not source.is_file():
        raise ValueError("The source file does not exist.")
    if not output_base.strip():
        raise ValueError("Choose an output base path for the BMP chunks.")
    if chunk_size_mb < 1:
        raise ValueError("Chunk size must be at least 1 MiB.")
    if num_threads < 0:
        raise ValueError("Thread count cannot be negative.")
    return EncodeRequest(str(source), str(Path(output_base).expanduser()), chunk_size_mb, num_threads)


def validate_decode_request(input_image_path: str, output_dir: str, num_threads: int) -> DecodeRequest:
    chunk_path = Path(input_image_path).expanduser()
    if not input_image_path.strip():
        raise ValueError("Choose a BMP chunk file to decode.")
    if not chunk_path.is_file():
        raise ValueError("The selected BMP chunk file does not exist.")
    if chunk_path.suffix.lower() != ".bmp":
        raise ValueError("The decode input must be a .bmp chunk file.")
    if not output_dir.strip():
        raise ValueError("Choose an output directory for the restored file.")
    if num_threads < 0:
        raise ValueError("Thread count cannot be negative.")
    return DecodeRequest(str(chunk_path), str(Path(output_dir).expanduser()), num_threads)
