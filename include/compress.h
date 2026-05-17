#ifndef PLAINSIGHT_COMPRESS_H
#define PLAINSIGHT_COMPRESS_H

#include <stddef.h>
#include <stdint.h>

#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

// Compresses one bounded payload with zstd into caller-owned storage
plainsight_error plainsight_compress_zstd(const uint8_t *input,
                          size_t input_len,
                          uint8_t *output,
                          size_t output_cap,
                          size_t *output_len);

// Compresses with an explicit zstd level for auto-selection trials
plainsight_error plainsight_compress_zstd_level(const uint8_t *input,
                                size_t input_len,
                                int compression_level,
                                uint8_t *output,
                                size_t output_cap,
                                size_t *output_len);

// Decompresses one zstd frame after validating the declared frame size
plainsight_error plainsight_decompress_zstd(const uint8_t *input,
                            size_t input_len,
                            uint8_t *output,
                            size_t output_cap,
                            size_t *output_len);

#ifdef __cplusplus
}
#endif

#endif
