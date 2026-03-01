#pragma once
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <pictobyte/BmpFormat.h>

namespace pb {

// ── RAII wrapper ─────────────────────────────────────────────────────────────

namespace detail {

struct FileCloser {
    void operator()(std::FILE* f) const noexcept { if (f) std::fclose(f); }
};
using UniqueFile = std::unique_ptr<std::FILE, FileCloser>;

[[nodiscard]] inline UniqueFile open_file(std::string_view path, const char* mode) {
    UniqueFile f(std::fopen(std::string(path).c_str(), mode));
    if (!f) throw std::runtime_error("Cannot open file: " + std::string(path));
    return f;
}

// Checked wrappers — throw rather than silently ignore I/O errors.
inline void checked_write(std::FILE* f, const void* buf, std::size_t n, std::string_view path) {
    if (std::fwrite(buf, 1, n, f) != n)
        throw std::runtime_error("Write error: " + std::string(path));
}

inline void checked_read(std::FILE* f, void* buf, std::size_t n, std::string_view path) {
    if (std::fread(buf, 1, n, f) != n)
        throw std::runtime_error("Read error: " + std::string(path));
}

} // namespace detail

// ─────────────────────────────────────────────────────────────────────────────
// BmpWriter
//
// Encodes raw bytes into 24-bit RGB pixels and writes a bottom-up BMP file.
// Width must be a multiple of 4 so row stride (w*3) is always 4-byte aligned
// — no BMP row-padding bytes are needed.
// ─────────────────────────────────────────────────────────────────────────────
class BmpWriter {
public:
    /**
     * @param path     Destination file path.
     * @param payload  Raw bytes to encode (3 bytes → 1 pixel; last pixel zero-padded).
     * @param size     Byte count of `payload`.
     * @param w        Image width in pixels (must be a multiple of 4).
     * @param h        Image height in pixels.
     */
    static void write(std::string_view path,
                      const std::uint8_t* payload, std::size_t size,
                      std::int32_t w, std::int32_t h)
    {
        const std::size_t row_stride     = static_cast<std::size_t>(w) * 3;
        const std::size_t pixel_data_size = row_stride * static_cast<std::size_t>(h);

        // Build headers with designated defaults from BmpFormat.h.
        BmpFileHeader fh;
        fh.bf_size     = static_cast<std::uint32_t>(BMP_HEADER_SIZE + pixel_data_size);
        fh.bf_off_bits = 54;

        BmpInfoHeader ih;
        ih.bi_width      = w;
        ih.bi_height     = h;   // positive → bottom-up storage
        ih.bi_size_image = static_cast<std::uint32_t>(pixel_data_size);

        const auto f = detail::open_file(path, "wb");

        detail::checked_write(f.get(), &fh, sizeof(fh), path);
        detail::checked_write(f.get(), &ih, sizeof(ih), path);

        // Write rows bottom-up (BMP convention): logical row h-1 first.
        std::vector<std::uint8_t> row(row_stride, 0u);

        for (std::int32_t y = h - 1; y >= 0; --y) {
            const std::size_t row_byte_offset = static_cast<std::size_t>(y) * row_stride;

            // How many payload bytes fall inside this row?
            const std::size_t copy_start = std::min(row_byte_offset, size);
            const std::size_t copy_end   = std::min(row_byte_offset + row_stride, size);
            const std::size_t copy_len   = copy_end - copy_start;

            if (copy_len > 0)
                std::memcpy(row.data(), payload + copy_start, copy_len);

            // Zero-pad any trailing bytes (handles partial last pixel).
            if (copy_len < row_stride)
                std::memset(row.data() + copy_len, 0, row_stride - copy_len);

            detail::checked_write(f.get(), row.data(), row_stride, path);
        }
        // UniqueFile destructor closes the file.
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// BmpReader
//
// Reads a 24-bit BMP written by BmpWriter and returns the packed pixel bytes
// in top-to-bottom, left-to-right order.
// ─────────────────────────────────────────────────────────────────────────────
class BmpReader {
public:
    /**
     * @param path  Source BMP file.
     * @param out   Receives the raw pixel bytes (w * h * 3 bytes, top-to-bottom).
     */
    static void read(std::string_view path, std::vector<std::uint8_t>& out) {
        const auto f = detail::open_file(path, "rb");

        BmpFileHeader fh;
        BmpInfoHeader ih;
        detail::checked_read(f.get(), &fh, sizeof(fh), path);
        detail::checked_read(f.get(), &ih, sizeof(ih), path);

        if (fh.bf_type != 0x4D42)
            throw std::runtime_error("Not a BMP file: " + std::string(path));

        // Negative height signals a top-down bitmap.
        const bool     bottom_up = ih.bi_height > 0;
        const std::int32_t w     = ih.bi_width;
        const std::int32_t h     = bottom_up ? ih.bi_height : -ih.bi_height;

        // Row stride must be rounded up to the nearest 4-byte boundary.
        const std::size_t row_stride = (static_cast<std::size_t>(w) * 3 + 3u) & ~3u;

        out.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3, 0u);

        std::fseek(f.get(), static_cast<long>(fh.bf_off_bits), SEEK_SET);

        std::vector<std::uint8_t> row(row_stride);

        for (std::int32_t file_row = 0; file_row < h; ++file_row) {
            // Map file row index to logical (top-down) row index.
            const std::int32_t logical_row = bottom_up ? (h - 1 - file_row) : file_row;

            detail::checked_read(f.get(), row.data(), row_stride, path);

            const std::size_t dst_offset =
                static_cast<std::size_t>(logical_row) * static_cast<std::size_t>(w) * 3;

            // Each row contains exactly w*3 meaningful bytes; padding bytes follow.
            std::memcpy(out.data() + dst_offset, row.data(), static_cast<std::size_t>(w) * 3);
        }
    }
};

} // namespace pb