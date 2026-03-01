#include "headers/Decoder.h"
#include "headers/BmpIO.h"
#include <pictobyte/BmpFormat.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace pb {

// ── Internal types ────────────────────────────────────────────────────────────

/** Metadata decoded from the ChunkMeta header embedded in each BMP. */
struct ParsedMeta {
    std::uint32_t chunk_index{};
    std::uint32_t total_chunks{};
    std::uint64_t file_total_size{};
    std::uint32_t raw_data_size{};   ///< payload bytes in this chunk
    std::uint32_t chunk_capacity{};  ///< max payload bytes in any non-last chunk
    std::string   orig_filename;
};

// ── File I/O helpers ──────────────────────────────────────────────────────────

namespace detail {

/// Cross-platform large-file seek.
inline void seek_file(std::FILE* f, std::uint64_t offset) {
#if defined(_WIN32)
    if (_fseeki64(f, static_cast<std::int64_t>(offset), SEEK_SET) != 0)
        throw std::runtime_error("Seek failed");
#else
    if (fseeko(f, static_cast<off_t>(offset), SEEK_SET) != 0)
        throw std::runtime_error("Seek failed");
#endif
}

using UniqueFile = std::unique_ptr<std::FILE, FileCloser>;

[[nodiscard]] inline UniqueFile open_file(const std::string& path, const char* mode) {
    UniqueFile f(std::fopen(path.c_str(), mode));
    if (!f) throw std::runtime_error("Cannot open file: " + path);
    return f;
}

} // namespace detail

// ── Metadata parsing ──────────────────────────────────────────────────────────

[[nodiscard]] static bool parse_meta(const std::vector<std::uint8_t>& pixels,
                                     ParsedMeta& out)
{
    if (pixels.size() < CHUNK_META_BASE_SIZE) return false;

    ChunkMeta cm{};
    std::memcpy(&cm, pixels.data(), CHUNK_META_BASE_SIZE);

    constexpr std::uint8_t kMagic[4] = { 'P', 'B', 'C', '2' };
    if (std::memcmp(cm.magic, kMagic, sizeof(kMagic)) != 0) return false;

    const std::size_t fn_len = cm.filename_len;
    if (CHUNK_META_BASE_SIZE + fn_len > pixels.size()) return false;

    out = {
        .chunk_index    = cm.chunk_index,
        .total_chunks   = cm.total_chunks,
        .file_total_size = cm.file_total_size,
        .raw_data_size  = cm.raw_data_size,
        .chunk_capacity = cm.chunk_capacity,
        .orig_filename  = { reinterpret_cast<const char*>(
                                pixels.data() + CHUNK_META_BASE_SIZE), fn_len }
    };
    return true;
}

// ── Chunk file discovery ──────────────────────────────────────────────────────

/**
 * Parse "myfile_3of10.bmp" → base="myfile", idx=2 (0-based), total=10.
 * Returns false if the filename does not match the expected pattern.
 */
[[nodiscard]] static bool parse_chunk_filename(const fs::path& p,
                                               std::string&    base_out,
                                               std::uint32_t&  idx_out,
                                               std::uint32_t&  total_out)
{
    // Compiled once per process; pattern is immutable.
    static const std::regex kPattern(R"(^(.+)_(\d+)of(\d+)$)");

    std::smatch m;
    const std::string stem = p.stem().string();
    if (!std::regex_match(stem, m, kPattern)) return false;

    base_out  = m[1].str();
    idx_out   = static_cast<std::uint32_t>(std::stoul(m[2].str())) - 1u; // → 0-based
    total_out = static_cast<std::uint32_t>(std::stoul(m[3].str()));
    return true;
}

/**
 * Build the ordered list of all chunk paths on disk.
 * Throws if any expected chunk file is absent.
 */
[[nodiscard]] static std::vector<fs::path> discover_chunks(const fs::path& any_chunk,
                                                           std::uint32_t   total_chunks,
                                                           std::string_view base_name)
{
    const fs::path dir = any_chunk.parent_path();
    std::vector<fs::path> paths(total_chunks);

    for (std::uint32_t i = 0; i < total_chunks; ++i) {
        std::ostringstream oss;
        oss << base_name << '_' << (i + 1) << "of" << total_chunks << ".bmp";
        paths[i] = dir / oss.str();
        if (!fs::exists(paths[i]))
            throw std::runtime_error("Missing chunk file: " + paths[i].string());
    }
    return paths;
}

// ── Pre-allocation ────────────────────────────────────────────────────────────

/** Punch the output file to its final size so parallel writes land at valid offsets. */
static void preallocate_output(const std::string& path, std::uint64_t size) {
    auto f = detail::open_file(path, "wb");
    if (size > 0) {
        detail::seek_file(f.get(), size - 1);
        std::fputc(0, f.get());
    }
}

// ── Decoder::decode ───────────────────────────────────────────────────────────

void Decoder::decode(const std::string& any_chunk_path,
                     const std::string& output_dir,
                     unsigned int       num_threads,
                     const Logger&      logger)
{
    const fs::path chunk_p(any_chunk_path);

    std::string   base_name;
    std::uint32_t first_idx{}, total_chunks{};

    if (!parse_chunk_filename(chunk_p, base_name, first_idx, total_chunks))
        throw std::runtime_error("Cannot parse chunk filename: " + any_chunk_path);

    logger.logf("Detected base='", base_name, "', total chunks=", total_chunks);

    // Read the provided chunk's metadata to learn the original filename / size.
    std::vector<std::uint8_t> probe_pixels;
    BmpReader::read(chunk_p.string(), probe_pixels);

    ParsedMeta probe;
    if (!parse_meta(probe_pixels, probe))
        throw std::runtime_error("Invalid or corrupt BMP metadata in: " + any_chunk_path);

    logger.logf("Original filename: ", probe.orig_filename);
    logger.logf("Total file size:   ", probe.file_total_size, " bytes");
    logger.logf("Chunk capacity:    ", probe.chunk_capacity, " bytes");

    const auto chunk_files = discover_chunks(chunk_p, total_chunks, base_name);

    // Prepare output.
    const fs::path  out_dir(output_dir);
    fs::create_directories(out_dir);
    const std::string out_path = (out_dir / probe.orig_filename).string();

    preallocate_output(out_path, probe.file_total_size);
    logger.logf("Output pre-allocated: ", out_path);

    // --- Parallel decode -------------------------------------------------------
    // Each chunk writes to a non-overlapping byte range of the output file.
    // We hold a single open FILE* and serialise seek+write under write_mutex —
    // cheaper than opening and closing the file once per chunk.

    const unsigned int eff_threads = num_threads
        ? num_threads : std::thread::hardware_concurrency();

    std::mutex write_mutex;
    auto out_file = detail::open_file(out_path, "r+b");

    ThreadPool pool(eff_threads, static_cast<std::size_t>(eff_threads) * 2);

    for (std::uint32_t ci = 0; ci < total_chunks; ++ci) {
        pool.submit([&, ci] {
            std::vector<std::uint8_t> pixels;
            BmpReader::read(chunk_files[ci].string(), pixels);

            ParsedMeta cm;
            if (!parse_meta(pixels, cm))
                throw std::runtime_error("Bad metadata in chunk " + std::to_string(ci));

            const std::size_t   meta_size    = CHUNK_META_BASE_SIZE + cm.orig_filename.size();
            const std::uint8_t* raw_ptr      = pixels.data() + meta_size;
            const std::size_t   raw_size     = cm.raw_data_size;
            const std::uint64_t write_offset =
                static_cast<std::uint64_t>(cm.chunk_index) *
                static_cast<std::uint64_t>(cm.chunk_capacity);

            {
                std::lock_guard lk(write_mutex);
                detail::seek_file(out_file.get(), write_offset);
                if (std::fwrite(raw_ptr, 1, raw_size, out_file.get()) != raw_size)
                    throw std::runtime_error("Write failed for chunk " + std::to_string(ci));
            }

            logger.logf("Decoded chunk ", cm.chunk_index + 1, '/', total_chunks,
                        " (", raw_size, " bytes) @ offset ", write_offset);
        });
    }

    pool.wait_all();
    logger.logf("Decoding complete. Output: ", out_path);
}

} // namespace pb