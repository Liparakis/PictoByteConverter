#pragma once
#include <cstdint>
#include <utility>

namespace pb {

// ─────────────────────────────────────────────────────────────────────────────
// Metadata header embedded at the very start of every BMP pixel data region.
// Fixed binary layout (little-endian) – do not reorder fields.
// ─────────────────────────────────────────────────────────────────────────────
#pragma pack(push, 1)
struct ChunkMeta {
    uint8_t  magic[4];          // "PBC2"
    uint32_t chunk_index;       // 0-based
    uint32_t total_chunks;
    uint64_t file_total_size;   // original file size in bytes
    uint32_t raw_data_size;     // bytes of original payload in THIS chunk
    uint32_t chunk_capacity;    // max raw bytes per non-last chunk (= file_offset / chunk_index)
    uint8_t  filename_len;      // length of original filename (max 255)
    // followed by filename_len bytes of filename (UTF-8, no null terminator)
};
#pragma pack(pop)

static_assert(sizeof(ChunkMeta) == 4+4+4+8+4+4+1, "ChunkMeta layout changed");
constexpr size_t CHUNK_META_BASE_SIZE = sizeof(ChunkMeta); // 29 bytes
constexpr size_t MAX_FILENAME_LEN     = 255;

// ─────────────────────────────────────────────────────────────────────────────
// BMP file-header layout (Windows BITMAPFILEHEADER + BITMAPINFOHEADER).
// We write a 24-bpp uncompressed BMP with no colour table.
// ─────────────────────────────────────────────────────────────────────────────
#pragma pack(push, 1)
struct BmpFileHeader {
    uint16_t bf_type    = 0x4D42; // "BM"
    uint32_t bf_size    = 0;      // total file size
    uint16_t bf_rsvd1   = 0;
    uint16_t bf_rsvd2   = 0;
    uint32_t bf_off_bits = 54;    // offset to pixel data
};

struct BmpInfoHeader {
    uint32_t bi_size          = 40;
    int32_t  bi_width         = 0;
    int32_t  bi_height        = 0;
    uint16_t bi_planes        = 1;
    uint16_t bi_bit_count     = 24;
    uint32_t bi_compression   = 0;
    uint32_t bi_size_image    = 0;
    int32_t  bi_x_pels_per_meter = 0;
    int32_t  bi_y_pels_per_meter = 0;
    uint32_t bi_clr_used      = 0;
    uint32_t bi_clr_important = 0;
};
#pragma pack(pop)

constexpr size_t BMP_HEADER_SIZE = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader); // 54 bytes

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Integer ceiling sqrt via Newton's method — no floating-point conversion.
[[nodiscard]] constexpr int32_t isqrt_ceil(size_t n) noexcept {
    if (n <= 1) return static_cast<int32_t>(n);
    size_t x = n;
    size_t y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    // x = floor(sqrt(n)); bump to ceil when n is not a perfect square.
    return static_cast<int32_t>(x * x >= n ? x : x + 1);
}

/**
 * Compute {width, height} to store `payload_bytes` at 3 bytes/pixel.
 * Width is rounded up to a multiple of 4 so BMP row stride is 4-byte aligned.
 */
[[nodiscard]] constexpr std::pair<int32_t, int32_t>
optimal_dims(size_t payload_bytes) noexcept {
    const size_t pixels = (payload_bytes + 2) / 3; // ceil(payload_bytes / 3)
    int32_t w = isqrt_ceil(pixels);
    if (w % 4 != 0) w += (4 - w % 4);
    const auto h = static_cast<int32_t>((pixels + w - 1) / w);
    return {w, h};
}

/**
 * Byte size of a 24-bpp BMP on disk for given dimensions (row-padded to 4 bytes).
 */
[[nodiscard]] constexpr size_t bmp_file_size(int32_t w, int32_t h) noexcept {
    const size_t row_stride = ((static_cast<size_t>(w) * 3 + 3) / 4) * 4;
    return BMP_HEADER_SIZE + row_stride * static_cast<size_t>(h);
}

} // namespace pb
