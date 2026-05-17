// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>

#include <zstd.h>

#include "../include/compress.h"
#include "../include/io.h"

plainsight_error plainsight_compress_zstd_level(const uint8_t *input,
                                size_t input_len,
                                int compression_level,
                                uint8_t *output,
                                size_t output_cap,
                                size_t *output_len) {
    size_t compressed_len = 0u;

    if (input == NULL || output == NULL || output_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (input_len > PLAINSIGHT_MAX_SINGLE_PAYLOAD_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    compressed_len = ZSTD_compress(output, output_cap, input, input_len, compression_level);
    if (ZSTD_isError(compressed_len) != 0u) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    if (compressed_len > PLAINSIGHT_MAX_SINGLE_PAYLOAD_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    *output_len = compressed_len;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_compress_zstd(const uint8_t *input,
                          size_t input_len,
                          uint8_t *output,
                          size_t output_cap,
                          size_t *output_len) {
    // Level 3 is zstd's default balance of speed and compression
    return plainsight_compress_zstd_level(input, input_len, 3, output, output_cap, output_len);
}

plainsight_error plainsight_decompress_zstd(const uint8_t *input,
                            size_t input_len,
                            uint8_t *output,
                            size_t output_cap,
                            size_t *output_len) {
    unsigned long long frame_content_size = 0ULL;
    size_t decompressed_len = 0u;

    if (input == NULL || output == NULL || output_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // This first pass accepts only frames with a known original size
    frame_content_size = ZSTD_getFrameContentSize(input, input_len);
    if (frame_content_size == ZSTD_CONTENTSIZE_ERROR || frame_content_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    if (frame_content_size > (unsigned long long)output_cap ||
        frame_content_size > (unsigned long long)PLAINSIGHT_MAX_SINGLE_PAYLOAD_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    decompressed_len = ZSTD_decompress(output, output_cap, input, input_len);
    if (ZSTD_isError(decompressed_len) != 0u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    if ((unsigned long long)decompressed_len != frame_content_size) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    *output_len = decompressed_len;
    return PLAINSIGHT_OK;
}
