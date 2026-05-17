// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include "../../include/embed/embed.h"

plainsight_error plainsight_embed_payload(plainsight_embed_method method,
                          uint8_t *cover,
                          size_t cover_len,
                          uint8_t cover_channel_stride,
                          const uint8_t *payload,
                          size_t payload_len,
                          const uint8_t seed[32]) {
    // Thin dispatcher keeps mode selection in one safe place
    switch (method) {
        case PLAINSIGHT_EMBED_LSB:
            // LSB mode is the only production path in this build
            return plainsight_embed_lsb_payload(cover, cover_len, cover_channel_stride, payload, payload_len, seed);
        default:
            // Unknown methods are rejected instead of silently defaulting
            return PLAINSIGHT_ERR_ARGS;
    }
}

plainsight_error plainsight_extract_payload(plainsight_embed_method method,
                            const uint8_t *cover,
                            size_t cover_len,
                            uint8_t cover_channel_stride,
                            uint8_t *payload_out,
                            size_t payload_cap,
                            size_t *payload_len,
                            const uint8_t seed[32]) {
    // Extraction uses the same mode so mapping logic stays symmetric
    switch (method) {
        case PLAINSIGHT_EMBED_LSB:
            // LSB extraction mirrors exact placement and bit ordering from hide
            return plainsight_extract_lsb_payload(
                cover, cover_len, cover_channel_stride, payload_out, payload_cap, payload_len, seed);
        default:
            return PLAINSIGHT_ERR_ARGS;
    }
}
