// main.cpp — lightweight CLI for the PictoByteConverter library
//
// Usage:
//   pictobyte_cli encode <input_file> <output_base> [chunk_mb] [threads]
//   pictobyte_cli decode <any_chunk.bmp> <output_dir> [threads]
//
// Examples:
//   pictobyte_cli encode D:\movie.mkv D:\out\movie 9 4
//   pictobyte_cli decode D:\out\movie_1of57.bmp D:\restored\ 4

#include <pictobyte/PictoByteAPI.h>
#include <cstdio>
#include <cstdlib>
#include <string>

static void log_handler(const char* msg, void* /*user_data*/) {
    printf("[pictobyte] %s\n", msg);
    fflush(stdout);
}

static void print_usage(const char* argv0) {
    printf(
        "PictoByteConverter v%s\n"
        "\nUsage:\n"
        "  %s encode <input_file> <output_base> [chunk_mb=9] [threads=0]\n"
        "  %s decode <any_chunk.bmp> <output_dir>  [threads=0]\n",
        pb_version(), argv0, argv0
    );
}

int main(const int argc, char** argv) {
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string mode(argv[1]);

    if (mode == "encode") {
        const char* input   = argv[2];
        const char* out_base = argv[3];
        const unsigned chunk_mb   = (argc > 4) ? static_cast<unsigned>(std::atoi(argv[4])) : 9u;
        const unsigned threads    = (argc > 5) ? static_cast<unsigned>(std::atoi(argv[5])) : 0u;

        int rc = pb_encode(input, out_base, chunk_mb, threads, log_handler, nullptr);
        if (rc != 0) {
            fprintf(stderr, "Error: %s\n", pb_last_error());
            return rc;
        }
        printf("Encoding complete.\n");

    } else if (mode == "decode") {
        const char* in_chunk = argv[2];
        const char* out_dir  = argv[3];
        const unsigned threads     = (argc > 4) ? static_cast<unsigned>(std::atoi(argv[4])) : 0u;

        int rc = pb_decode(in_chunk, out_dir, threads, log_handler, nullptr);
        if (rc != 0) {
            fprintf(stderr, "Error: %s\n", pb_last_error());
            return rc;
        }
        printf("Decoding complete.\n");

    } else {
        fprintf(stderr, "Unknown mode: %s\n", mode.c_str());
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
