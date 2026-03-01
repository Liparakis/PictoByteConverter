#include "headers/Encoder.h"
#include "headers/BmpIO.h"
#include "headers/ThreadPool.h"
#include <pictobyte/BmpFormat.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace pb {
    // ── File I/O helpers ──────────────────────────────────────────────────────────

    namespace detail {
        using UniqueFile = std::unique_ptr<std::FILE, FileCloser>;

        [[nodiscard]] inline UniqueFile open_file(const std::string &path, const char *mode) {
            UniqueFile f(std::fopen(path.c_str(), mode));
            if (!f) throw std::runtime_error("Cannot open file: " + path);
            return f;
        }

        inline void seek_file(std::FILE *f, std::uint64_t offset) {
#if defined(_WIN32)
            if (_fseeki64(f, static_cast<std::int64_t>(offset), SEEK_SET) != 0)
#else
            if (fseeko(f, static_cast<off_t>(offset), SEEK_SET) != 0)
#endif
                throw std::runtime_error("Seek failed");
        }

        [[nodiscard]] inline std::uint64_t file_size(std::FILE *f) {
#if defined(_WIN32)
            _fseeki64(f, 0, SEEK_END);
            const std::int64_t sz = _ftelli64(f);
            _fseeki64(f, 0, SEEK_SET);
#else
            fseeko(f, 0, SEEK_END);
            const std::int64_t sz = static_cast<std::int64_t>(ftello(f));
            fseeko(f, 0, SEEK_SET);
#endif
            if (sz < 0) throw std::runtime_error("Cannot determine file size");
            return static_cast<std::uint64_t>(sz);
        }
    } // namespace detail

    // ── Per-chunk encoding ────────────────────────────────────────────────────────

    /**
     * Read one chunk from `src` at `file_offset`, serialise metadata directly
     * into the pixel buffer (no intermediate meta vector), and write a BMP.
     *
     * Each worker thread opens its own file handle, so no mutex is needed.
     */
    static void encode_chunk(std::FILE *src,
                             const std::uint64_t file_offset,
                             const std::uint32_t raw_size,
                             const std::uint32_t chunk_index,
                             const std::uint32_t total_chunks,
                             const std::uint64_t file_total_size,
                             const std::uint32_t chunk_capacity,
                             const std::string_view orig_filename,
                             const std::string &out_path) {
        constexpr std::uint8_t kMagic[4] = {'P', 'B', 'C', '2'};
        const auto fn_len = static_cast<std::uint8_t>(
            std::min(orig_filename.size(), MAX_FILENAME_LEN));
        const std::size_t meta_sz = CHUNK_META_BASE_SIZE + fn_len;

        const std::size_t total_payload = meta_sz + raw_size;
        const auto [w, h] = optimal_dims(total_payload);
        const std::size_t pixel_bytes =
                static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3;

        // Build payload in-place: ChunkMeta + filename + raw file data.
        std::vector<std::uint8_t> payload(pixel_bytes, 0u);

        ChunkMeta cm{};
        std::memcpy(cm.magic, kMagic, sizeof(kMagic));
        cm.chunk_index     = chunk_index;
        cm.total_chunks    = total_chunks;
        cm.file_total_size = file_total_size;
        cm.raw_data_size   = raw_size;
        cm.chunk_capacity  = chunk_capacity;
        cm.filename_len    = fn_len;

        std::memcpy(payload.data(), &cm, CHUNK_META_BASE_SIZE);
        std::memcpy(payload.data() + CHUNK_META_BASE_SIZE, orig_filename.data(), fn_len);

        if (raw_size > 0) {
            detail::seek_file(src, file_offset);
            const std::size_t n = std::fread(payload.data() + meta_sz, 1, raw_size, src);
            if (n != raw_size)
                throw std::runtime_error("Short read from input at chunk " +
                                         std::to_string(chunk_index));
        }

        BmpWriter::write(out_path, payload.data(), total_payload, w, h);
    }

    // ── Encoder::encode ───────────────────────────────────────────────────────────

    void Encoder::encode(const std::string &input_path,
                         const std::string &output_base,
                         unsigned int chunk_size_mb,
                         unsigned int num_threads,
                         const Logger &log) {
        if (chunk_size_mb < 1)
            throw std::runtime_error("chunk_size_mb must be >= 1");

        const auto src = detail::open_file(input_path, "rb");
        const std::uint64_t fsize = detail::file_size(src.get());

        const std::string orig_filename = fs::path(input_path).filename().string();
        const std::size_t fn_len = std::min(orig_filename.size(), MAX_FILENAME_LEN);
        const std::size_t meta_sz = CHUNK_META_BASE_SIZE + fn_len;

        const std::size_t target_bmp_bytes =
                static_cast<std::size_t>(chunk_size_mb) * 1024ULL * 1024ULL;

        const std::size_t max_payload =
                ((target_bmp_bytes - BMP_HEADER_SIZE) / 3) * 3;

        if (max_payload <= meta_sz)
            throw std::runtime_error("chunk_size_mb too small to hold metadata");

        const std::size_t raw_capacity = max_payload - meta_sz;
        const std::uint64_t total_chunks =
                fsize == 0 ? 1 : (fsize + raw_capacity - 1) / raw_capacity;

        const unsigned int eff_threads =
                num_threads ? num_threads : std::thread::hardware_concurrency();

        log.logf("Input:          ", input_path, " (", fsize, " bytes)");
        log.logf("Chunk capacity: ", raw_capacity, " bytes");
        log.logf("Total chunks:   ", total_chunks);
        log.logf("Threads:        ", eff_threads);

        ThreadPool pool(eff_threads, static_cast<std::size_t>(eff_threads) * 2);

        // Pre-format total_chunks string once — avoids per-iteration to_string.
        const std::string tc_str = std::to_string(total_chunks);

        for (std::uint64_t ci = 0; ci < total_chunks; ++ci) {
            const std::uint64_t offset = ci * raw_capacity;
            const std::uint64_t remaining = (fsize > offset) ? fsize - offset : 0u;
            const auto this_raw = static_cast<std::uint32_t>(
                std::min(remaining, static_cast<std::uint64_t>(raw_capacity)));

            // String concatenation — no ostringstream heap overhead.
            std::string out_path;
            out_path.reserve(output_base.size() + tc_str.size() + 16);
            out_path += output_base;
            out_path += '_';
            out_path += std::to_string(ci + 1);
            out_path += "of";
            out_path += tc_str;
            out_path += ".bmp";

            pool.submit([=, &log] {
                const auto tf = detail::open_file(input_path, "rb");
                encode_chunk(tf.get(), offset, this_raw,
                             static_cast<std::uint32_t>(ci),
                             static_cast<std::uint32_t>(total_chunks),
                             fsize,
                             static_cast<std::uint32_t>(raw_capacity),
                             orig_filename, out_path);
            });
        }

        pool.wait_all();
        log.logf("Encoding complete. ", total_chunks, " chunk(s) written.");
    }
} // namespace pb
