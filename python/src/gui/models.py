from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class EncodeRequest:
    input_path: str
    output_base: str
    chunk_size_mb: int
    num_threads: int


@dataclass(frozen=True)
class DecodeRequest:
    input_image_path: str
    output_dir: str
    num_threads: int


@dataclass(frozen=True)
class JobResult:
    mode: str
    summary: str
