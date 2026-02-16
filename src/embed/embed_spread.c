#include <stddef.h>
#include <stdint.h>

#include "../../include/embed/embed.h"

plainsight_error plainsight_embed_spread_payload(uint8_t *cover,
                                 size_t cover_len,
                                 uint8_t cover_channel_stride,
                                 const uint8_t *payload,
                                 size_t payload_len,
                                 const uint8_t seed[32]) {
    // Spread-spectrum mode placeholder
    // Current builds expose only LSB in the UI to avoid runtime surprises
    (void)cover;
    (void)cover_len;
    (void)cover_channel_stride;
    (void)payload;
    (void)payload_len;
    (void)seed;
    return PLAINSIGHT_ERR_UNSUPPORTED;
}

plainsight_error plainsight_extract_spread_payload(const uint8_t *cover,
                                   size_t cover_len,
                                   uint8_t cover_channel_stride,
                                   uint8_t *payload_out,
                                   size_t payload_cap,
                                   size_t *payload_len,
                                   const uint8_t seed[32]) {
    // Placeholder keeps API stable while method implementation is pending
    (void)cover;
    (void)cover_len;
    (void)cover_channel_stride;
    (void)payload_out;
    (void)payload_cap;
    (void)payload_len;
    (void)seed;
    return PLAINSIGHT_ERR_UNSUPPORTED;
}
