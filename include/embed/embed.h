#ifndef PLAINSIGHT_EMBED_H
#define PLAINSIGHT_EMBED_H

#include <stddef.h>
#include <stdint.h>

#include "../error.h"

#ifdef __cplusplus
extern "C" {
#endif

// Embedding modes kept explicit for forward compatibility
typedef enum plainsight_embed_method {
    PLAINSIGHT_EMBED_LSB = 0,
    PLAINSIGHT_EMBED_SPREAD = 1,
    PLAINSIGHT_EMBED_INVALID = 255
} plainsight_embed_method;

// Writes payload bits into cover bytes using selected method
// cover_channel_stride is the byte distance to a neighboring sample in the same channel
plainsight_error plainsight_embed_payload(plainsight_embed_method method,
                          uint8_t *cover,
                          size_t cover_len,
                          uint8_t cover_channel_stride,
                          const uint8_t *payload,
                          size_t payload_len,
                          const uint8_t seed[32]);

// Recovers payload bytes from a modified cover using selected method
plainsight_error plainsight_extract_payload(plainsight_embed_method method,
                            const uint8_t *cover,
                            size_t cover_len,
                            uint8_t cover_channel_stride,
                            uint8_t *payload_out,
                            size_t payload_cap,
                            size_t *payload_len,
                            const uint8_t seed[32]);

// LSB backend with deterministic keyed placement
plainsight_error plainsight_embed_lsb_payload(uint8_t *cover,
                              size_t cover_len,
                              uint8_t cover_channel_stride,
                              const uint8_t *payload,
                              size_t payload_len,
                              const uint8_t seed[32]);

plainsight_error plainsight_extract_lsb_payload(const uint8_t *cover,
                                size_t cover_len,
                                uint8_t cover_channel_stride,
                                uint8_t *payload_out,
                                size_t payload_cap,
                                size_t *payload_len,
                                const uint8_t seed[32]);

// Placeholder for a future spread-spectrum backend
plainsight_error plainsight_embed_spread_payload(uint8_t *cover,
                                 size_t cover_len,
                                 uint8_t cover_channel_stride,
                                 const uint8_t *payload,
                                 size_t payload_len,
                                 const uint8_t seed[32]);

plainsight_error plainsight_extract_spread_payload(const uint8_t *cover,
                                   size_t cover_len,
                                   uint8_t cover_channel_stride,
                                   uint8_t *payload_out,
                                   size_t payload_cap,
                                   size_t *payload_len,
                                   const uint8_t seed[32]);

#ifdef __cplusplus
}
#endif

#endif
