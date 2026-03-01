#pragma once
#include <pictobyte/Logger.h>
#include "ThreadPool.h"
#include <string>

namespace pb {

struct Decoder {
    /**
     * Reconstruct the original file from a set of BMP chunk images.
     * @param any_chunk_path  Path to any one of the BMP chunk files.
     * @param output_dir      Directory to write the reconstructed file.
     * @param num_threads     0 = auto.
     * @param log             Logger instance.
     */
    static void decode(const std::string& any_chunk_path,
                        const std::string& output_dir,
                        unsigned int       num_threads,
                        const Logger&      log);
};

} // namespace pb
