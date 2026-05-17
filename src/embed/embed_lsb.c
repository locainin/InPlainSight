// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include "../../include/embed/embed.h"

#define PLAINSIGHT_LSB_PREFIX_BITS 64u
#define PLAINSIGHT_LSB_SEED_DOMAIN_MIN 1u

// LSB embedding uses keyed placement plus a stable texture preference
// The placement must be identical before and after embedding, so all scores ignore the bit being written

// Seed bytes drive a deterministic threshold so hide and extract use the same slots
static uint16_t plainsight_lsb_texture_threshold(const uint8_t seed_bytes[32]) {
    // Threshold only controls priority, not hard capacity
    return (uint16_t)(4u + (uint16_t)(seed_bytes[24] % 5u));
}

static uint64_t plainsight_lsb_read_seed_u64(const uint8_t seed_bytes[32], size_t byte_offset) {
    uint64_t seed_value = 0u;
    size_t byte_index = 0u;

    for (byte_index = 0u; byte_index < 8u; byte_index++) {
        seed_value |= ((uint64_t)seed_bytes[byte_offset + byte_index]) << (8u * byte_index);
    }

    return seed_value;
}

static uint64_t plainsight_lsb_rotate_left_u64(uint64_t value, unsigned int shift_count) {
    unsigned int normalized_shift = shift_count & 63u;

    if (normalized_shift == 0u) {
        return value;
    }
    return (value << normalized_shift) | (value >> (64u - normalized_shift));
}

static uint64_t plainsight_lsb_mix_seed_u64(const uint8_t seed_bytes[32],
                                    uint64_t domain_size,
                                    uint8_t domain_label) {
    uint64_t lane0 = plainsight_lsb_read_seed_u64(seed_bytes, 0u);
    uint64_t lane1 = plainsight_lsb_read_seed_u64(seed_bytes, 8u);
    uint64_t lane2 = plainsight_lsb_read_seed_u64(seed_bytes, 16u);
    uint64_t lane3 = plainsight_lsb_read_seed_u64(seed_bytes, 24u);
    uint64_t mixed_value = 0u;

    // All four seed lanes are folded into both permutation values
    // The labels keep step and bias from becoming the same value on repeated inputs
    mixed_value = lane0;
    mixed_value ^= plainsight_lsb_rotate_left_u64(lane1, 17u);
    mixed_value ^= plainsight_lsb_rotate_left_u64(lane2, 31u);
    mixed_value ^= plainsight_lsb_rotate_left_u64(lane3, 47u);
    mixed_value ^= (uint64_t)domain_label * UINT64_C(0x9E3779B97F4A7C15);

    return mixed_value % domain_size;
}

static uint64_t plainsight_lsb_greatest_common_divisor(uint64_t first_value, uint64_t second_value) {
    while (second_value != 0u) {
        uint64_t remainder = first_value % second_value;
        first_value = second_value;
        second_value = remainder;
    }
    return first_value;
}

static plainsight_error plainsight_lsb_compute_permutation(const uint8_t seed_bytes[32],
                                           uint64_t domain_size,
                                           uint64_t *step_out,
                                           uint64_t *bias_out) {
    uint64_t permutation_step = 0u;
    uint64_t permutation_bias = 0u;

    if (seed_bytes == NULL || step_out == NULL || bias_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (domain_size < PLAINSIGHT_LSB_SEED_DOMAIN_MIN) {
        return PLAINSIGHT_ERR_ARGS;
    }

    permutation_step = plainsight_lsb_mix_seed_u64(seed_bytes, domain_size, 0xA5u);
    if (permutation_step == 0u) {
        permutation_step = 1u;
    }

    // Coprime step guarantees a full permutation without repeated indices
    while (plainsight_lsb_greatest_common_divisor(permutation_step, domain_size) != 1u) {
        permutation_step++;
        if (permutation_step >= domain_size) {
            permutation_step = 1u;
        }
    }

    permutation_bias = plainsight_lsb_mix_seed_u64(seed_bytes, domain_size, 0x5Au);
    *step_out = permutation_step;
    *bias_out = permutation_bias;
    return PLAINSIGHT_OK;
}

static uint64_t plainsight_lsb_advance_permutation(uint64_t current_index,
                                           uint64_t permutation_step,
                                           uint64_t domain_size) {
    uint64_t wrap_threshold = domain_size - permutation_step;

    // permutation_step is always in 1..domain_size-1 except the single-slot case
    // The subtraction form avoids multiplication and modulo in the pixel loop
    if (permutation_step == domain_size) {
        return 0u;
    }
    if (current_index >= wrap_threshold) {
        return current_index - wrap_threshold;
    }
    return current_index + permutation_step;
}

static uint8_t plainsight_lsb_abs_difference(uint8_t first_value, uint8_t second_value) {
    if (first_value >= second_value) {
        return (uint8_t)(first_value - second_value);
    }
    return (uint8_t)(second_value - first_value);
}

static uint16_t plainsight_lsb_texture_score(const uint8_t *cover_bytes,
                                     size_t cover_length,
                                     uint64_t cover_index,
                                     uint8_t cover_channel_stride) {
    uint8_t current_value = 0u;
    uint16_t score_total = 0u;
    uint64_t channel_step = (uint64_t)cover_channel_stride;

    // Mask LSB so score stays stable after embedding
    current_value = (uint8_t)(cover_bytes[cover_index] & 0xFEu);

    if (cover_index >= channel_step) {
        uint8_t left_same_channel = (uint8_t)(cover_bytes[cover_index - channel_step] & 0xFEu);
        score_total = (uint16_t)(score_total + plainsight_lsb_abs_difference(current_value, left_same_channel));
    }

    if (cover_index + channel_step < (uint64_t)cover_length) {
        uint8_t right_same_channel = (uint8_t)(cover_bytes[cover_index + channel_step] & 0xFEu);
        score_total = (uint16_t)(score_total + plainsight_lsb_abs_difference(current_value, right_same_channel));
    }

    return score_total;
}

static int plainsight_lsb_slot_is_usable(const uint8_t *cover_bytes,
                                 size_t cover_length,
                                 uint64_t cover_index,
                                 uint8_t cover_channel_stride,
                                 uint16_t minimum_texture_score) {
    uint16_t slot_score = plainsight_lsb_texture_score(cover_bytes, cover_length, cover_index, cover_channel_stride);
    return slot_score >= minimum_texture_score;
}

static int plainsight_lsb_slot_matches_phase(unsigned int phase_index, int slot_is_high_texture) {
    if (phase_index == 0u) {
        return slot_is_high_texture != 0;
    }
    return slot_is_high_texture == 0;
}

static uint8_t plainsight_lsb_length_bit(size_t payload_length, uint64_t bit_index) {
    size_t length_byte_index = (size_t)(bit_index / 8u);
    unsigned int bit_offset = (unsigned int)(bit_index % 8u);
    uint64_t payload_length_u64 = (uint64_t)payload_length;
    uint8_t length_byte = (uint8_t)((payload_length_u64 >> (8u * length_byte_index)) & 0xFFu);
    return (uint8_t)((((unsigned int)length_byte) >> bit_offset) & 1u);
}

static uint8_t plainsight_lsb_payload_bit(const uint8_t *payload_bytes, uint64_t payload_bit_index) {
    size_t payload_byte_index = (size_t)(payload_bit_index / 8u);
    unsigned int payload_bit_offset = (unsigned int)(payload_bit_index % 8u);
    // Read one payload bit from little-endian bit order within each payload byte
    return (uint8_t)((((unsigned int)payload_bytes[payload_byte_index]) >> payload_bit_offset) & 1u);
}

static void plainsight_lsb_write_payload_bit(uint8_t *payload_bytes,
                                     uint64_t payload_bit_index,
                                     uint8_t bit_value) {
    size_t payload_byte_index = (size_t)(payload_bit_index / 8u);
    unsigned int payload_bit_offset = (unsigned int)(payload_bit_index % 8u);
    uint8_t bit_mask = (uint8_t)(1u << payload_bit_offset);

    // Toggle only the addressed bit so neighboring bits remain unchanged
    if (bit_value == 0u) {
        payload_bytes[payload_byte_index] &= (uint8_t)(~bit_mask);
    } else {
        payload_bytes[payload_byte_index] |= bit_mask;
    }
}

plainsight_error plainsight_embed_lsb_payload(uint8_t *cover,
                              size_t cover_len,
                              uint8_t cover_channel_stride,
                              const uint8_t *payload,
                              size_t payload_len,
                              const uint8_t seed[32]) {
    uint64_t required_bit_count = 0u;
    uint16_t minimum_texture_score = 0u;
    uint64_t permutation_step = 0u;
    uint64_t permutation_bias = 0u;
    uint64_t permutation_index = 0u;
    uint64_t embedded_bit_count = 0u;
    unsigned int phase_index = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    if (cover == NULL || payload == NULL || seed == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (cover_len == 0u || cover_channel_stride == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Prefix is 64 bits, so payload length must fit both prefix and checked arithmetic
#if SIZE_MAX > UINT64_MAX
    if (payload_len > (size_t)UINT64_MAX) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
#endif
    if ((uint64_t)payload_len > (UINT64_MAX / 8u) - 8u) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    required_bit_count = ((uint64_t)payload_len + 8u) * 8u;
    minimum_texture_score = plainsight_lsb_texture_threshold(seed);
    if (required_bit_count > (uint64_t)cover_len) {
        return PLAINSIGHT_ERR_CAPACITY;
    }

    result_code = plainsight_lsb_compute_permutation(seed,
                                             (uint64_t)cover_len,
                                             &permutation_step,
                                             &permutation_bias);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    for (phase_index = 0u; phase_index < 2u && embedded_bit_count < required_bit_count; phase_index++) {
        uint64_t cover_index = permutation_bias;

        for (permutation_index = 0u;
             permutation_index < (uint64_t)cover_len && embedded_bit_count < required_bit_count;
             permutation_index++) {
            uint64_t selected_cover_index = cover_index;
            int slot_is_high_texture = plainsight_lsb_slot_is_usable(cover,
                                                             cover_len,
                                                             selected_cover_index,
                                                             cover_channel_stride,
                                                             minimum_texture_score);
            uint8_t bit_to_embed = 0u;

            cover_index = plainsight_lsb_advance_permutation(cover_index, permutation_step, (uint64_t)cover_len);

            // Phase 0 consumes textured slots first, phase 1 consumes the rest
            if (!plainsight_lsb_slot_matches_phase(phase_index, slot_is_high_texture)) {
                continue;
            }

            if (embedded_bit_count < PLAINSIGHT_LSB_PREFIX_BITS) {
                bit_to_embed = plainsight_lsb_length_bit(payload_len, embedded_bit_count);
            } else {
                bit_to_embed = plainsight_lsb_payload_bit(payload, embedded_bit_count - PLAINSIGHT_LSB_PREFIX_BITS);
            }

            // Least significant bit (LSB) stores one message bit per cover byte
            cover[selected_cover_index] = (uint8_t)((cover[selected_cover_index] & 0xFEu) | bit_to_embed);
            embedded_bit_count++;
        }
    }

    if (embedded_bit_count != required_bit_count) {
        return PLAINSIGHT_ERR_CAPACITY;
    }

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_extract_lsb_payload(const uint8_t *cover,
                                size_t cover_len,
                                uint8_t cover_channel_stride,
                                uint8_t *payload_out,
                                size_t payload_cap,
                                size_t *payload_len,
                                const uint8_t seed[32]) {
    uint16_t minimum_texture_score = 0u;
    uint64_t permutation_step = 0u;
    uint64_t permutation_bias = 0u;
    uint64_t permutation_index = 0u;
    uint64_t observed_slot_count = 0u;
    uint64_t encoded_payload_length = 0u;
    uint64_t required_bit_count = 0u;
    uint64_t payload_byte_index = 0u;
    unsigned int phase_index = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    if (cover == NULL || payload_out == NULL || payload_len == NULL || seed == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (cover_len == 0u || cover_channel_stride == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    minimum_texture_score = plainsight_lsb_texture_threshold(seed);
    if ((uint64_t)cover_len < PLAINSIGHT_LSB_PREFIX_BITS) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    result_code = plainsight_lsb_compute_permutation(seed,
                                             (uint64_t)cover_len,
                                             &permutation_step,
                                             &permutation_bias);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // First pass reads the 64-bit encoded payload length prefix
    for (phase_index = 0u; phase_index < 2u && observed_slot_count < PLAINSIGHT_LSB_PREFIX_BITS; phase_index++) {
        uint64_t cover_index = permutation_bias;

        for (permutation_index = 0u;
             permutation_index < (uint64_t)cover_len && observed_slot_count < PLAINSIGHT_LSB_PREFIX_BITS;
             permutation_index++) {
            uint64_t selected_cover_index = cover_index;
            int slot_is_high_texture = plainsight_lsb_slot_is_usable(cover,
                                                             cover_len,
                                                             selected_cover_index,
                                                             cover_channel_stride,
                                                             minimum_texture_score);
            uint8_t embedded_bit = 0u;

            cover_index = plainsight_lsb_advance_permutation(cover_index, permutation_step, (uint64_t)cover_len);

            if (!plainsight_lsb_slot_matches_phase(phase_index, slot_is_high_texture)) {
                continue;
            }

            // Read one LSB from the selected cover slot
            embedded_bit = (uint8_t)(cover[selected_cover_index] & 1u);
            encoded_payload_length |= ((uint64_t)embedded_bit) << observed_slot_count;
            observed_slot_count++;
        }
    }

    if (encoded_payload_length > (uint64_t)payload_cap) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    if (encoded_payload_length > (UINT64_MAX / 8u) - 8u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    required_bit_count = (encoded_payload_length + 8u) * 8u;
    if (required_bit_count > (uint64_t)cover_len) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    for (payload_byte_index = 0u; payload_byte_index < encoded_payload_length; payload_byte_index++) {
        payload_out[payload_byte_index] = 0u;
    }

    // Second pass rebuilds payload bytes using the same two-phase slot ordering
    observed_slot_count = 0u;
    for (phase_index = 0u; phase_index < 2u && observed_slot_count < required_bit_count; phase_index++) {
        uint64_t cover_index = permutation_bias;

        for (permutation_index = 0u;
             permutation_index < (uint64_t)cover_len && observed_slot_count < required_bit_count;
             permutation_index++) {
            uint64_t selected_cover_index = cover_index;
            int slot_is_high_texture = plainsight_lsb_slot_is_usable(cover,
                                                             cover_len,
                                                             selected_cover_index,
                                                             cover_channel_stride,
                                                             minimum_texture_score);
            uint8_t embedded_bit = 0u;

            cover_index = plainsight_lsb_advance_permutation(cover_index, permutation_step, (uint64_t)cover_len);

            if (!plainsight_lsb_slot_matches_phase(phase_index, slot_is_high_texture)) {
                continue;
            }

            embedded_bit = (uint8_t)(cover[selected_cover_index] & 1u);
            if (observed_slot_count >= PLAINSIGHT_LSB_PREFIX_BITS) {
                uint64_t payload_bit_index = observed_slot_count - PLAINSIGHT_LSB_PREFIX_BITS;
                plainsight_lsb_write_payload_bit(payload_out, payload_bit_index, embedded_bit);
            }

            observed_slot_count++;
        }
    }

    if (observed_slot_count != required_bit_count) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    *payload_len = (size_t)encoded_payload_length;
    return PLAINSIGHT_OK;
}
