// API.cpp — implements the extern "C" functions declared in PictoByteAPI.h
#include <pictobyte/PictoByteAPI.h>
#include <pictobyte/Logger.h>
#include "headers/Encoder.h"
#include "headers/Decoder.h"

#include <functional>
#include <string>
#include <string_view>

// ── Version ───────────────────────────────────────────────────────────────────

static constexpr char k_version[] = "2.0.0";

// ── Per-thread error storage ──────────────────────────────────────────────────

// Each thread maintains its own error string so callers can check
// pb_last_error() without racing against other threads.
static thread_local std::string tl_last_error;

// ── Internal helpers ──────────────────────────────────────────────────────────

/**
 * Execute `fn`, capture any exception message into tl_last_error, and return
 * 0 on success / -1 on failure. Guarantees that tl_last_error is always left
 * in a consistent state regardless of the code path taken.
 */
static int run_safe(const std::function<void()> &fn) noexcept {
    try {
        fn();
        tl_last_error.clear();
        return 0;
    } catch (const std::exception& e) {
        tl_last_error = e.what();
    } catch (...) {
        tl_last_error = "Unknown error";
    }
    return -1;
}

/**
 * Build a pb::Logger that forwards messages through the C callback.
 * Returns a silent no-op Logger when `cb` is null.
 */
[[nodiscard]] static pb::Logger make_logger(pb_log_callback_t cb, void* user_data) {
    if (!cb) return {};
    return pb::Logger([cb, user_data](std::string_view msg) {
        cb(std::string(msg).c_str(), user_data);
    });
}

// ── Public C API ──────────────────────────────────────────────────────────────
//
// NOTE: PictoByteAPI.h already wraps all declarations in extern "C".
//       Do NOT add another extern "C" block here; that would give MSVC/clang
//       a "dllimport cannot be applied to a definition" error.

PB_API int pb_encode(
    const char*       input_path,
    const char*       output_base,
    const unsigned int      chunk_size_mb,
    const unsigned int      num_threads,
    const pb_log_callback_t log_cb,
    void*             log_user_data)
{
    return run_safe([&] {
        const auto logger = make_logger(log_cb, log_user_data);
        pb::Encoder::encode(input_path, output_base,
                            chunk_size_mb, num_threads, logger);
    });
}

PB_API int pb_decode(
    const char*       input_image_path,
    const char*       output_dir,
    const unsigned int      num_threads,
    const pb_log_callback_t log_cb,
    void*             log_user_data)
{
    return run_safe([&] {
        const auto logger = make_logger(log_cb, log_user_data);
        pb::Decoder::decode(input_image_path, output_dir,
                            num_threads, logger);
    });
}

PB_API const char* pb_last_error() {
    return tl_last_error.c_str();
}

PB_API const char* pb_version() {
    return k_version;
}