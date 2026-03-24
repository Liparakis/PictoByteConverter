#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
  #if defined(PICTOBYTE_STATIC)
    #define PB_API
  #elif defined(PICTOBYTE_EXPORTS)
    #define PB_API __declspec(dllexport)
  #else
    #define PB_API __declspec(dllimport)
  #endif
#else
  #define PB_API __attribute__((visibility("default")))
#endif

/**
 * @brief Callback type for log messages.
 * @param message Null-terminated UTF-8 log string.
 * @param user_data Opaque pointer passed back to the caller.
 */
typedef void (*pb_log_callback_t)(const char* message, void* user_data);

/**
 * @brief Convert a binary file to one or more BMP images.
 *
 * Each output image will contain at most `chunk_size_mb` megabytes of raw data
 * (plus a small embedded metadata header). The actual BMP file will be slightly
 * larger than `chunk_size_mb` due to the BMP file header and the metadata block.
 *
 * Output files are named: <output_base>_<i>of<N>.bmp  (1-indexed)
 *
 * @param input_path       Path to the source file.
 * @param output_base      Base path / name for output images (directory must exist).
 * @param chunk_size_mb    Maximum raw data per image chunk, in **mebibytes** (MiB).
 *                         Must be >= 1.
 * @param num_threads      Worker threads to use. 0 = auto-detect.
 * @param log_cb           Optional progress/log callback. Pass NULL to suppress.
 * @param log_user_data    Opaque pointer forwarded to every `log_cb` call.
 * @return 0 on success, non-zero error code on failure.
 */
PB_API int pb_encode(
    const char* input_path,
    const char* output_base,
    unsigned int chunk_size_mb,
    unsigned int num_threads,
    pb_log_callback_t log_cb,
    void* log_user_data
);

/**
 * @brief Reconstruct the original file from a set of BMP images.
 *
 * Supply any one of the chunk files; the library discovers the rest automatically
 * by reading the embedded metadata and scanning the same directory.
 *
 * @param input_image_path  Path to any one of the chunk BMP files.
 * @param output_dir        Directory where the reconstructed file will be written.
 *                          Uses the original filename stored in the metadata.
 * @param num_threads        Worker threads to use. 0 = auto-detect.
 * @param log_cb            Optional log callback. Pass NULL to suppress.
 * @param log_user_data     Opaque pointer forwarded to every `log_cb` call.
 * @return 0 on success, non-zero error code on failure.
 */
PB_API int pb_decode(
    const char* input_image_path,
    const char* output_dir,
    unsigned int num_threads,
    pb_log_callback_t log_cb,
    void* log_user_data
);

/**
 * @brief Return a null-terminated string describing the last error on this thread.
 * The returned pointer is valid until the next library call on the same thread.
 */
PB_API const char* pb_last_error(void);

/**
 * @brief Return the library version string, e.g. "2.0.0".
 */
PB_API const char* pb_version(void);

#ifdef __cplusplus
} // extern "C"
#endif
