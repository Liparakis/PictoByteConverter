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
// Builds the entire file in a single contiguous buffer and issues one fwrite
// call — eliminates per-row I/O syscalls.
// ─────────────────────────────────────────────────────────────────────────────
class BmpWriter {
public:
    static void write(std::string_view path,
                      const std::uint8_t* payload, std::size_t size,
                      std::int32_t w, std::int32_t h)
    {
        const std::size_t row_stride      = static_cast<std::size_t>(w) * 3;
        const std::size_t pixel_data_size = row_stride * static_cast<std::size_t>(h);
        const std::size_t total_file_size = BMP_HEADER_SIZE + pixel_data_size;

        // Build the complete BMP file in one contiguous buffer.
        std::vector<std::uint8_t> buf(total_file_size, 0u);

        BmpFileHeader fh;
        fh.bf_size     = static_cast<std::uint32_t>(total_file_size);
        fh.bf_off_bits = 54;

        BmpInfoHeader ih;
        ih.bi_width      = w;
        ih.bi_height     = h;   // positive → bottom-up storage
        ih.bi_size_image = static_cast<std::uint32_t>(pixel_data_size);

        std::memcpy(buf.data(), &fh, sizeof(fh));
        std::memcpy(buf.data() + sizeof(fh), &ih, sizeof(ih));

        // Place payload rows in BMP bottom-up order: logical row h-1 first.
        std::uint8_t* pixels = buf.data() + BMP_HEADER_SIZE;

        for (std::int32_t bmp_row = 0; bmp_row < h; ++bmp_row) {
            const auto payload_row  = static_cast<std::size_t>(h - 1 - bmp_row);
            const std::size_t src_off = payload_row * row_stride;
            const std::size_t dst_off = static_cast<std::size_t>(bmp_row) * row_stride;

            const std::size_t copy_start = std::min(src_off, size);
            const std::size_t copy_end   = std::min(src_off + row_stride, size);
            const std::size_t copy_len   = copy_end - copy_start;

            if (copy_len > 0)
                std::memcpy(pixels + dst_off, payload + copy_start, copy_len);
            // Remainder stays zero from buffer initialization.
        }

        // Single I/O call for the entire file.
        const auto f = detail::open_file(path, "wb");
        detail::checked_write(f.get(), buf.data(), total_file_size, path);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// BmpReader
//
// Reads a 24-bit BMP and returns packed pixel bytes in top-to-bottom order.
// Reads all pixel data in a single fread, then rearranges rows in memory.
// ─────────────────────────────────────────────────────────────────────────────
class BmpReader {
public:
    static void read(std::string_view path, std::vector<std::uint8_t>& out) {
        const auto f = detail::open_file(path, "rb");

        BmpFileHeader fh;
        BmpInfoHeader ih;
        detail::checked_read(f.get(), &fh, sizeof(fh), path);
        detail::checked_read(f.get(), &ih, sizeof(ih), path);

        if (fh.bf_type != 0x4D42)
            throw std::runtime_error("Not a BMP file: " + std::string(path));

        const bool     bottom_up = ih.bi_height > 0;
        const std::int32_t w     = ih.bi_width;
        const std::int32_t h     = bottom_up ? ih.bi_height : -ih.bi_height;

        const std::size_t row_bytes  = static_cast<std::size_t>(w) * 3;
        const std::size_t row_stride = (row_bytes + 3u) & ~3u;
        const std::size_t pixel_data_size = row_stride * static_cast<std::size_t>(h);

        // Seek to pixel data start.
        std::fseek(f.get(), static_cast<long>(fh.bf_off_bits), SEEK_SET);

        // Single bulk read of all pixel data.
        std::vector<std::uint8_t> raw(pixel_data_size);
        detail::checked_read(f.get(), raw.data(), pixel_data_size, path);

        // Rearrange rows into top-down logical order, stripping any padding.
        out.resize(row_bytes * static_cast<std::size_t>(h));

        for (std::int32_t file_row = 0; file_row < h; ++file_row) {
            const std::int32_t logical_row = bottom_up ? (h - 1 - file_row) : file_row;

            const std::size_t src_off = static_cast<std::size_t>(file_row)    * row_stride;
            const std::size_t dst_off = static_cast<std::size_t>(logical_row) * row_bytes;

            std::memcpy(out.data() + dst_off, raw.data() + src_off, row_bytes);
        }
    }
};

} // namespace pb
