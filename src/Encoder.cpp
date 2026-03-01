#include "headers/Encoder.h"
#include "headers/BmpIO.h"
#include "headers/ThreadPool.h"
#include <pictobyte/BmpFormat.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace pb {
    // ── File I/O helpers (mirrors Decoder pattern) ────────────────────────────────

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

    // ── Metadata serialisation ────────────────────────────────────────────────────

    [[nodiscard]] static std::vector<std::uint8_t>
    build_meta(const std::uint32_t chunk_index,
               const std::uint32_t total_chunks,
               const std::uint64_t file_total_size,
               const std::uint32_t raw_data_size,
               const std::uint32_t chunk_capacity,
               const std::string_view orig_filename) {
        constexpr std::uint8_t kMagic[4] = {'P', 'B', 'C', '2'};

        const auto fn_len = static_cast<std::uint8_t>(
            std::min(orig_filename.size(), MAX_FILENAME_LEN));

        ChunkMeta cm{};
        std::memcpy(cm.magic, kMagic, sizeof(kMagic));
        cm.chunk_index = chunk_index;
        cm.total_chunks = total_chunks;
        cm.file_total_size = file_total_size;
        cm.raw_data_size = raw_data_size;
        cm.chunk_capacity = chunk_capacity;
        cm.filename_len = fn_len;

        std::vector<std::uint8_t> meta(CHUNK_META_BASE_SIZE + fn_len);
        std::memcpy(meta.data(), &cm, CHUNK_META_BASE_SIZE);
        std::memcpy(meta.data() + CHUNK_META_BASE_SIZE, orig_filename.data(), fn_len);
        return meta;
    }

    // ── Per-chunk encoding ────────────────────────────────────────────────────────

    /**
     * Read one chunk from `src` at `file_offset`, prepend its metadata header,
     * and write the result to a BMP at `out_path`.
     *
     * Each worker thread opens its own file handle, so no mutex is needed here.
     */
    static void encode_chunk(std::FILE *src,
                             const std::uint64_t file_offset,
                             const std::uint32_t raw_size,
                             const std::uint32_t chunk_index,
                             const std::uint32_t total_chunks,
                             const std::uint64_t file_total_size,
                             const std::uint32_t chunk_capacity,
                             const std::string_view orig_filename,
                             const std::string &out_path,
                             const Logger &logger) {
        const auto meta = build_meta(chunk_index, total_chunks,
                                     file_total_size, raw_size,
                                     chunk_capacity, orig_filename);

        const std::size_t total_payload = meta.size() + raw_size;
        const auto [w, h] = optimal_dims(total_payload);

        const std::size_t pixel_bytes =
                static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3;

        std::vector<std::uint8_t> payload(pixel_bytes, 0u);
        std::memcpy(payload.data(), meta.data(), meta.size());

        if (raw_size > 0) {
            detail::seek_file(src, file_offset);
            const std::size_t n = std::fread(payload.data() + meta.size(), 1, raw_size, src);
            if (n != raw_size)
                throw std::runtime_error("Short read from input at chunk " +
                                         std::to_string(chunk_index));
        }

        BmpWriter::write(out_path, payload.data(), total_payload, w, h);
        logger.logf("Encoded chunk ", chunk_index + 1, '/', total_chunks,
                    " -> ", out_path, " (", raw_size, " bytes)");
    }

    // ── Encoder::encode ───────────────────────────────────────────────────────────

    void Encoder::encode(const std::string &input_path,
                         const std::string &output_base,
                         unsigned int chunk_size_mb,
                         unsigned int num_threads,
                         const Logger &log) {
        if (chunk_size_mb < 1)
            throw std::runtime_error("chunk_size_mb must be >= 1");

        // Measure the input file; we keep the handle open only for measurement —
        // worker threads each open their own handle to avoid contention.
        const auto src = detail::open_file(input_path, "rb");
        const std::uint64_t fsize = detail::file_size(src.get());

        const std::string orig_filename = fs::path(input_path).filename().string();
        const std::size_t fn_len = std::min(orig_filename.size(), MAX_FILENAME_LEN);
        const std::size_t meta_sz = CHUNK_META_BASE_SIZE + fn_len;

        // Max payload per BMP, aligned down to whole pixel triplets.
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

        // Bound queue depth to 2× thread count to cap peak RAM usage.
        ThreadPool pool(eff_threads, static_cast<std::size_t>(eff_threads) * 2);

        for (std::uint64_t ci = 0; ci < total_chunks; ++ci) {
            const std::uint64_t offset = ci * raw_capacity;
            const std::uint64_t remaining = (fsize > offset) ? fsize - offset : 0u;
            const std::uint32_t this_raw = static_cast<std::uint32_t>(
                std::min(remaining, static_cast<std::uint64_t>(raw_capacity)));

            std::ostringstream oss;
            oss << output_base << '_' << (ci + 1) << "of" << total_chunks << ".bmp";
            std::string out_path = oss.str();

            pool.submit([=, &log] {
                // Each worker opens its own file handle — no shared-state I/O.
                const auto tf = detail::open_file(input_path, "rb");
                encode_chunk(tf.get(), offset, this_raw,
                             static_cast<std::uint32_t>(ci),
                             static_cast<std::uint32_t>(total_chunks),
                             fsize,
                             static_cast<std::uint32_t>(raw_capacity),
                             orig_filename, out_path, log);
                // tf closed automatically on scope exit, even on exception.
            });
        }

        pool.wait_all();
        log.logf("Encoding complete. ", total_chunks, " chunk(s) written.");
    }
} // namespace pb
