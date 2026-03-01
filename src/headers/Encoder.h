#pragma once
#include <pictobyte/Logger.h>
#include <string>

namespace pb {

struct Encoder {
    /**
     * Convert a binary file to one or more BMP images.
     * @param input_path    Raw input file.
     * @param output_base   Output path prefix (e.g. "D:/out/myfile").
     *                      Images will be named: myfile_1of3.bmp, etc.
     * @param chunk_size_mb Target BMP file size in MiB (>= 1).
     * @param num_threads   0 = auto.
     * @param log           Logger instance.
     */
    static void encode(const std::string& input_path,
                        const std::string& output_base,
                        unsigned int       chunk_size_mb,
                        unsigned int       num_threads,
                        const Logger&      log);
};

} // namespace pb
