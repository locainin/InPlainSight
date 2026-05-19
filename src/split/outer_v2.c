// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>

#include "../../include/split/outer_v2.h"

// Outer v2 is a split-aware container header that is embedded as plaintext bytes
// It is not secret, but it must be authenticated via AEAD AAD so it cannot be edited without detection

static const uint8_t PLAINSIGHT_SPLIT_MAGIC[PLAINSIGHT_CONTAINER_MAGIC_LEN] = {
    (uint8_t)'H', (uint8_t)'I', (uint8_t)'I', (uint8_t)'S',
    (uint8_t)'P', (uint8_t)'L', (uint8_t)'0', (uint8_t)'2'
};

static void plainsight_split_write_u16_le(uint8_t *out, uint16_t value) {
    // Little-endian encoding keeps the on-disk/on-wire format consistent across CPU types
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8u) & 0xFFu);
}

static void plainsight_split_write_u32_le(uint8_t *out, uint32_t value) {
    // Fixed-width writes avoid UB from unaligned casts
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8u) & 0xFFu);
    out[2] = (uint8_t)((value >> 16u) & 0xFFu);
    out[3] = (uint8_t)((value >> 24u) & 0xFFu);
}

static void plainsight_split_write_u64_le(uint8_t *out, uint64_t value) {
    size_t byte_index = 0u;
    // Byte loop keeps the encoding explicit and easy to audit
    for (byte_index = 0u; byte_index < 8u; byte_index++) {
        out[byte_index] = (uint8_t)((value >> (8u * byte_index)) & 0xFFu);
    }
}

static uint16_t plainsight_split_read_u16_le(const uint8_t *in) {
    // Reads mirror writes so pack/parse stay symmetric
    return (uint16_t)((uint16_t)in[0] | ((uint16_t)in[1] << 8u));
}

static uint32_t plainsight_split_read_u32_le(const uint8_t *in) {
    uint32_t value = 0u;
    // Shifts are done on unsigned types to avoid sign surprises
    value |= (uint32_t)in[0];
    value |= ((uint32_t)in[1] << 8u);
    value |= ((uint32_t)in[2] << 16u);
    value |= ((uint32_t)in[3] << 24u);
    return value;
}

static uint64_t plainsight_split_read_u64_le(const uint8_t *in) {
    uint64_t value = 0u;
    size_t byte_index = 0u;
    // Loop form makes the decode order obvious
    for (byte_index = 0u; byte_index < 8u; byte_index++) {
        value |= ((uint64_t)in[byte_index]) << (8u * byte_index);
    }
    return value;
}

static plainsight_error plainsight_split_outer_v2_validate(const plainsight_split_outer_v2 *outer) {
    if (outer == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    // Versioning is strict so older parsers do not mis-handle newer headers
    if (outer->version != PLAINSIGHT_SPLIT_OUTER_VERSION) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    // Flags are fail-closed so unknown bits do not silently change meaning
    if ((outer->flags & (uint8_t)(~PLAINSIGHT_SPLIT_FLAGS_KNOWN)) != 0u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    // Shard_count bounds prevent DoS-size manifest tables and index math bugs
    if (outer->shard_count == 0u || outer->shard_count > PLAINSIGHT_MAX_SHARDS) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    // Index must always be inside [0, shard_count)
    if (outer->shard_index >= outer->shard_count) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    // ciphertext_len is the encrypted blob length including the AEAD tag
    if (outer->ciphertext_len == 0u || outer->ciphertext_len > PLAINSIGHT_MAX_CIPHERTEXT_BYTES) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_split_aad_serialize_outer_v2(const plainsight_split_outer_v2 *outer,
                                         uint8_t *out,
                                         size_t out_cap,
                                         size_t *out_len) {
    size_t write_offset = 0u;
    size_t set_id_index = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    if (outer == NULL || out == NULL || out_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (out_cap < PLAINSIGHT_SPLIT_OUTER_FIXED_BYTES) {
        // Callers must size buffers for the fixed serialized layout
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    // This serializer is the canonical AAD byte layout for split v2
    // Hide and extract must both call this one function
    // If this layout ever changes, it must be guarded by a new outer version
    result_code = plainsight_split_outer_v2_validate(outer);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // Magic bytes are included in AAD so container type cannot be swapped
    for (set_id_index = 0u; set_id_index < PLAINSIGHT_CONTAINER_MAGIC_LEN; set_id_index++) {
        out[write_offset + set_id_index] = PLAINSIGHT_SPLIT_MAGIC[set_id_index];
    }
    write_offset += PLAINSIGHT_CONTAINER_MAGIC_LEN;

    out[write_offset++] = outer->version;
    out[write_offset++] = outer->flags;
    plainsight_split_write_u16_le(out + write_offset, outer->kdf_alg);
    write_offset += 2u;
    plainsight_split_write_u64_le(out + write_offset, outer->kdf_opslimit);
    write_offset += 8u;
    plainsight_split_write_u64_le(out + write_offset, outer->kdf_memlimit);
    write_offset += 8u;

    // Salt and nonce are public inputs needed to derive keys and decrypt
    for (set_id_index = 0u; set_id_index < 16u; set_id_index++) {
        out[write_offset + set_id_index] = outer->salt[set_id_index];
    }
    write_offset += 16u;
    for (set_id_index = 0u; set_id_index < 24u; set_id_index++) {
        out[write_offset + set_id_index] = outer->nonce[set_id_index];
    }
    write_offset += 24u;
    // set_id is not secret, it is used to prevent mixing shards from different runs
    for (set_id_index = 0u; set_id_index < PLAINSIGHT_SPLIT_SET_ID_BYTES; set_id_index++) {
        out[write_offset + set_id_index] = outer->set_id[set_id_index];
    }
    write_offset += PLAINSIGHT_SPLIT_SET_ID_BYTES;
    // index/count are authenticated so shards cannot be swapped or re-labeled
    plainsight_split_write_u32_le(out + write_offset, outer->shard_index);
    write_offset += 4u;
    plainsight_split_write_u32_le(out + write_offset, outer->shard_count);
    write_offset += 4u;
    // ciphertext_len is authenticated so truncation/extension is detected
    plainsight_split_write_u64_le(out + write_offset, outer->ciphertext_len);
    write_offset += 8u;

    *out_len = write_offset;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_split_outer_v2_pack(const plainsight_split_outer_v2 *outer,
                                const uint8_t *ciphertext,
                                size_t ciphertext_len,
                                uint8_t *out,
                                size_t out_cap,
                                size_t *out_len) {
    size_t header_len = 0u;
    size_t ciphertext_index = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    if (outer == NULL || ciphertext == NULL || out == NULL || out_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (ciphertext_len == 0u || ciphertext_len > PLAINSIGHT_MAX_CIPHERTEXT_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    if ((uint64_t)ciphertext_len != outer->ciphertext_len) {
        // Pack requires the header and ciphertext size to agree exactly
        return PLAINSIGHT_ERR_ARGS;
    }
    if (out_cap < PLAINSIGHT_SPLIT_OUTER_FIXED_BYTES || ciphertext_len > out_cap - PLAINSIGHT_SPLIT_OUTER_FIXED_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    // Serialize header first so callers can pass the same bytes as AEAD AAD
    result_code = plainsight_split_aad_serialize_outer_v2(outer, out, out_cap, &header_len);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // Ciphertext follows the fixed header directly
    for (ciphertext_index = 0u; ciphertext_index < ciphertext_len; ciphertext_index++) {
        out[header_len + ciphertext_index] = ciphertext[ciphertext_index];
    }

    *out_len = header_len + ciphertext_len;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_split_outer_v2_parse(const uint8_t *in,
                                 size_t in_len,
                                 plainsight_split_outer_v2 *outer,
                                 const uint8_t **ciphertext,
                                 size_t *ciphertext_len) {
    size_t read_offset = 0u;
    size_t byte_index = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    if (in == NULL || outer == NULL || ciphertext == NULL || ciphertext_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (in_len < PLAINSIGHT_SPLIT_OUTER_FIXED_BYTES) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Magic rejects non-split containers early
    // This check happens before any other field reads to keep parsing cheap on noise input
    for (byte_index = 0u; byte_index < PLAINSIGHT_CONTAINER_MAGIC_LEN; byte_index++) {
        if (in[read_offset + byte_index] != PLAINSIGHT_SPLIT_MAGIC[byte_index]) {
            return PLAINSIGHT_ERR_BAD_FORMAT;
        }
    }
    read_offset += PLAINSIGHT_CONTAINER_MAGIC_LEN;

    outer->version = in[read_offset++];
    outer->flags = in[read_offset++];
    outer->kdf_alg = plainsight_split_read_u16_le(in + read_offset);
    read_offset += 2u;
    outer->kdf_opslimit = plainsight_split_read_u64_le(in + read_offset);
    read_offset += 8u;
    outer->kdf_memlimit = plainsight_split_read_u64_le(in + read_offset);
    read_offset += 8u;

    for (byte_index = 0u; byte_index < 16u; byte_index++) {
        outer->salt[byte_index] = in[read_offset + byte_index];
    }
    read_offset += 16u;
    for (byte_index = 0u; byte_index < 24u; byte_index++) {
        outer->nonce[byte_index] = in[read_offset + byte_index];
    }
    read_offset += 24u;
    for (byte_index = 0u; byte_index < PLAINSIGHT_SPLIT_SET_ID_BYTES; byte_index++) {
        outer->set_id[byte_index] = in[read_offset + byte_index];
    }
    read_offset += PLAINSIGHT_SPLIT_SET_ID_BYTES;
    outer->shard_index = plainsight_split_read_u32_le(in + read_offset);
    read_offset += 4u;
    outer->shard_count = plainsight_split_read_u32_le(in + read_offset);
    read_offset += 4u;
    outer->ciphertext_len = plainsight_split_read_u64_le(in + read_offset);
    read_offset += 8u;

    // Validate fields before using ciphertext_len for bounds checks
    result_code = plainsight_split_outer_v2_validate(outer);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // Split v2 format requires exact-length match so trailing bytes are rejected
    if (outer->ciphertext_len > (uint64_t)(in_len - read_offset)) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    if ((uint64_t)read_offset + outer->ciphertext_len != (uint64_t)in_len) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Callers get a view into the original input buffer to avoid extra copies
    *ciphertext = in + read_offset;
    *ciphertext_len = (size_t)outer->ciphertext_len;
    return PLAINSIGHT_OK;
}
