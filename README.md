# PictoByteConverter v2.0.0

PictoByteConverter is a high-performance C++ library and command-line utility for converting arbitrary binary data into standard BMP images and reconstructing the original files. Version 2.0.0 represents a complete rewrite focusing on I/O throughput, minimal memory overhead, and robust multithreading.

## Key Features

- **High-Performance Multithreading**: Utilizes a bounded thread pool to parallelize encoding and decoding tasks.
- **Streaming I/O Architecture**: Processes files incrementally to ensure a low and stable RAM footprint, capable of handling multi-gigabyte files on resource-constrained systems.
- **Data Integrity**: Uses standard 24-bit 0x4D42 BMP formats with bit-perfect reconstruction.
- **Advanced Metadata**: Each image segment contains an embedded header (magic: `PBC2`) storing original filename, sequence index, total size, and chunk capacity.
- **Native Python Integration**: Includes a comprehensive Python wrapper leveraging `ctypes` for seamless integration without compiling C extensions.
- **C-Compatible API**: Clean `extern "C"` interface ensures compatibility with a wide range of host languages.

## Project Structure

```text
PictoByteConverter/
├── include/pictobyte/  # Public API headers (PictoByteAPI.h, BmpFormat.h, Logger.h)
├── src/                # Implementation source files
│   └── headers/        # Private internal headers (BmpIO.h, ThreadPool.h, etc.)
├── cli/                # Command-line interface implementation
├── python/             # Python wrapper module (pictobyte.py)
├── examples/           # Usage demonstrations
│   └── python/         # Python example script with round-trip verification
└── CMakeLists.txt      # Modern CMake build system
```

## Compilation

### Prerequisites
- CMake 3.15 or higher
- C++17 compliant compiler (GCC, Clang, or MSVC)
- Python 3.8+ (for bindings and examples)

### Build Instructions
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

The build produces:
- Windows: `bin/pictobyte.dll` or `bin/libpictobyte.dll` plus `bin/pictobyte_cli.exe`
- Linux: `bin/libpictobyte.so` plus `bin/pictobyte_cli`
- macOS: `bin/libpictobyte.dylib` plus `bin/pictobyte_cli`

## Usage

### Command-Line Interface (CLI)

The CLI tool allows batch processing of files from the shell.

**Encoding a file to images:**
```bash
# Usage: pictobyte_cli encode <input_file> <output_base> [chunk_mb=9] [threads=0]
pictobyte_cli encode large_data.bin processed/chunk 9 8
```

**Decoding images back to file:**
```bash
# Usage: pictobyte_cli decode <any_chunk.bmp> <output_dir> [threads=0]
# The library automatically discovers all related chunks in the same directory.
pictobyte_cli decode processed/chunk_1of12.bmp restored/
```

### Python API

The `PictoByteConverter` class provides a high-level interface with automatic library discovery.

```python
from python.pictobyte import PictoByteConverter

# Optional: verbose=True prints log messages from the C++ core to stdout
pb = PictoByteConverter(verbose=True)

# Encode a file
pb.encode("source.iso", "encoded/segment", chunk_size_mb=10, num_threads=0)

# Decode (input path can be any chunk in the sequence)
pb.decode("encoded/segment_1of5.bmp", "output_dir/")
```

### PySide6 Desktop GUI

The repo now includes a desktop app built on top of the same
Python wrapper and native library.

#### Features
- Encode and decode workflows in one window
- Drag-and-drop file and folder fields
- Live log output from the native library
- Persistent last-used paths and settings
- Optional explicit DLL override for packaged or custom builds

#### Run from source
1. Build the native library so `pictobyte.dll` exists in a build directory.
2. Install the GUI dependencies:
   ```bash
   python -m pip install ./python[build]
   ```
3. Launch the desktop app:
   ```bash
   python python/pictobyte_gui.py
   ```

#### Package the GUI
1. Build the native library first.
2. Run the staging and packaging script:
   ```bash
   python python/build_gui.py
   ```
3. The packaged app will be emitted under `python/bin/`:
   - Windows: `PictoByteStudio.exe`
   - Linux: `PictoByteStudio`
   - macOS: `PictoByteStudio.app`

The build script performs an explicit shared-library staging step before packaging so the
desktop app does not rely on implicit auto-discovery during bundling.

## Technical Implementation Details

### RAM Efficiency and Throughput
The core engine prioritizes memory safety. By utilizing a bounded queue in the `ThreadPool`, the application limits the number of chunks actively resident in memory. I/O operations are performed using row-based buffering inside the `BmpWriter` and `BmpReader` classes, ensuring that only small slices of pixel data are allocated at any given time.

### Pixel Alignment
To avoid the overhead of BMP row padding (which occurs when row widths are not multiples of four), PictoByte enforces image dimensions where the width is always a multiple of 4. Data is packed into 3-byte pixel triplets, ensuring a 1:1 mapping between payload segments and image data without bit-shifting overhead.

### Metadata Specification
Each segment starts with a fixed-base header:
1. Magic Sequence: `PBC2` (4 bytes)
2. Chunk Index (4 bytes, 0-indexed)
3. Total Chunks (4 bytes)
4. Original File Size (8 bytes)
5. Raw Data Size in this chunk (4 bytes)
6. Consistent Chunk Capacity (4 bytes)
7. Filename Length (1 byte)
8. Original Filename (Variable, UTF-8)

## License
Distributed under the MIT License. See `LICENSE.md` for details.
