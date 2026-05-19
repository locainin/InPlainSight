// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>

#include "../include/container.h"

static const uint8_t PLAINSIGHT_CONTAINER_MAGIC[PLAINSIGHT_CONTAINER_MAGIC_LEN] = {
    (uint8_t)'H', (uint8_t)'I', (uint8_t)'I', (uint8_t)'C',
    (uint8_t)'T', (uint8_t)'R', (uint8_t)'0', (uint8_t)'1'
};

static void plainsight_write_u16_le(uint8_t *output_bytes, uint16_t value) {
    output_bytes[0] = (uint8_t)(value & 0xFFu);
    output_bytes[1] = (uint8_t)((value >> 8u) & 0xFFu);
}

static void plainsight_write_u64_le(uint8_t *output_bytes, uint64_t value) {
    size_t byte_index = 0u;
    for (byte_index = 0u; byte_index < 8u; byte_index++) {
        output_bytes[byte_index] = (uint8_t)((value >> (8u * byte_index)) & 0xFFu);
    }
}

static uint16_t plainsight_read_u16_le(const uint8_t *input_bytes) {
    uint16_t decoded_value = 0u;
    decoded_value = (uint16_t)input_bytes[0];
    decoded_value |= (uint16_t)((uint16_t)input_bytes[1] << 8u);
    return decoded_value;
}

static uint64_t plainsight_read_u64_le(const uint8_t *input_bytes) {
    uint64_t decoded_value = 0u;
    size_t byte_index = 0u;
    for (byte_index = 0u; byte_index < 8u; byte_index++) {
        decoded_value |= ((uint64_t)input_bytes[byte_index]) << (8u * byte_index);
    }
    return decoded_value;
}

static plainsight_error plainsight_copy_bytes(uint8_t *destination_bytes,
                                              size_t destination_capacity,
                                              size_t *destination_offset,
                                              const uint8_t *source_bytes,
                                              size_t source_length) {
    size_t source_index = 0u;

    if (destination_bytes == NULL || destination_offset == NULL || (source_bytes == NULL && source_length > 0u)) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (*destination_offset > destination_capacity ||
        source_length > (destination_capacity - *destination_offset)) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    for (source_index = 0u; source_index < source_length; source_index++) {
        destination_bytes[*destination_offset + source_index] = source_bytes[source_index];
    }
    *destination_offset += source_length;
    return PLAINSIGHT_OK;
}

static plainsight_error plainsight_validate_embedded_name(const uint8_t *name, uint16_t name_len) {
    size_t name_index = 0u;

    if (name_len == 0u) {
        return PLAINSIGHT_OK;
    }
    if (name == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if ((name_len == 1u && name[0] == (uint8_t)'.') ||
        (name_len == 2u && name[0] == (uint8_t)'.' && name[1] == (uint8_t)'.')) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    for (name_index = 0u; name_index < (size_t)name_len; name_index++) {
        if (name[name_index] == (uint8_t)'/' ||
            name[name_index] == (uint8_t)'\\' ||
            name[name_index] == (uint8_t)'\0' ||
            name[name_index] < 0x20u) {
            return PLAINSIGHT_ERR_BAD_FORMAT;
        }
    }

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_container_pack_inner(const plainsight_inner_header *inner,
                                 uint8_t *out,
                                 size_t out_cap,
                                 size_t *out_len) {
    size_t write_offset = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    if (inner == NULL || out == NULL || out_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (inner->payload == NULL && inner->payload_len > 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (inner->payload_len > PLAINSIGHT_MAX_SINGLE_PAYLOAD_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    // Optional fields must be present when lengths are non-zero
    if ((inner->name == NULL && inner->name_len > 0u) || (inner->mime == NULL && inner->mime_len > 0u)) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (inner->name_len > PLAINSIGHT_MAX_FILENAME_BYTES || inner->mime_len > PLAINSIGHT_MAX_MIME_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    if (inner->compression != PLAINSIGHT_COMPRESSION_NONE && inner->compression != PLAINSIGHT_COMPRESSION_ZSTD) {
        return PLAINSIGHT_ERR_ARGS;
    }

    result_code = plainsight_validate_embedded_name(inner->name, inner->name_len);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    if (16u > out_cap) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    // Fixed-size prelude keeps parsing simple and predictable
    plainsight_write_u64_le(out + 0u, inner->payload_len);
    plainsight_write_u16_le(out + 8u, inner->name_len);
    plainsight_write_u16_le(out + 10u, inner->mime_len);
    out[12] = inner->compression;
    out[13] = 0u;
    out[14] = 0u;
    out[15] = 0u;
    write_offset = 16u;

    result_code = plainsight_copy_bytes(out, out_cap, &write_offset, inner->name, inner->name_len);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    result_code = plainsight_copy_bytes(out, out_cap, &write_offset, inner->mime, inner->mime_len);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    result_code = plainsight_copy_bytes(out, out_cap, &write_offset, inner->payload, (size_t)inner->payload_len);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    *out_len = write_offset;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_container_parse_inner(const uint8_t *in,
                                  size_t in_len,
                                  plainsight_inner_header *inner_view) {
    uint64_t payload_len = 0u;
    uint16_t name_len = 0u;
    uint16_t mime_len = 0u;
    size_t header_size = 16u;
    size_t total_metadata = 0u;
    plainsight_error result_code = PLAINSIGHT_OK;

    if (in == NULL || inner_view == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (in_len < header_size) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Header fields are little-endian and fixed width
    payload_len = plainsight_read_u64_le(in + 0u);
    name_len = plainsight_read_u16_le(in + 8u);
    mime_len = plainsight_read_u16_le(in + 10u);
    if (in[12] != PLAINSIGHT_COMPRESSION_NONE && in[12] != PLAINSIGHT_COMPRESSION_ZSTD) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    if (in[13] != 0u || in[14] != 0u || in[15] != 0u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    if (name_len > PLAINSIGHT_MAX_FILENAME_BYTES || mime_len > PLAINSIGHT_MAX_MIME_BYTES) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    total_metadata = (size_t)name_len + (size_t)mime_len;

    if (total_metadata > (in_len - header_size)) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    if (payload_len > (uint64_t)(in_len - header_size - total_metadata)) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Enforce exact length match so trailing garbage is rejected
    if ((uint64_t)header_size + (uint64_t)total_metadata + payload_len != (uint64_t)in_len) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    result_code = plainsight_validate_embedded_name(in + header_size, name_len);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // Views point into the original input buffer, no extra copy needed
    inner_view->compression = in[12];
    inner_view->name_len = name_len;
    inner_view->mime_len = mime_len;
    inner_view->payload_len = payload_len;
    inner_view->name = in + header_size;
    inner_view->mime = in + header_size + (size_t)name_len;
    inner_view->payload = in + header_size + (size_t)name_len + (size_t)mime_len;

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_container_pack_outer(const plainsight_outer_header *outer,
                                 const uint8_t *ciphertext,
                                 size_t ciphertext_len,
                                 uint8_t *out,
                                 size_t out_cap,
                                 size_t *out_len) {
    size_t byte_index = 0u;
    size_t write_offset = 0u;

    if (outer == NULL || ciphertext == NULL || out == NULL || out_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (outer->version != PLAINSIGHT_CONTAINER_VERSION) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (ciphertext_len < PLAINSIGHT_CONTAINER_AEAD_TAG_BYTES ||
        ciphertext_len > PLAINSIGHT_MAX_CIPHERTEXT_BYTES ||
        outer->ciphertext_len != (uint64_t)ciphertext_len) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    if (PLAINSIGHT_CONTAINER_OUTER_FIXED_BYTES > out_cap ||
        ciphertext_len > (out_cap - PLAINSIGHT_CONTAINER_OUTER_FIXED_BYTES)) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    // Magic bytes allow quick format check during extraction
    for (byte_index = 0u; byte_index < PLAINSIGHT_CONTAINER_MAGIC_LEN; byte_index++) {
        out[write_offset + byte_index] = PLAINSIGHT_CONTAINER_MAGIC[byte_index];
    }
    write_offset += PLAINSIGHT_CONTAINER_MAGIC_LEN;

    // Outer header is intentionally minimal to reduce leakage
    out[write_offset] = outer->version;
    write_offset += 1u;

    out[write_offset] = 0u;
    write_offset += 1u;

    plainsight_write_u16_le(out + write_offset, outer->kdf_alg);
    write_offset += 2u;

    plainsight_write_u64_le(out + write_offset, outer->kdf_opslimit);
    write_offset += 8u;

    plainsight_write_u64_le(out + write_offset, outer->kdf_memlimit);
    write_offset += 8u;

    for (byte_index = 0u; byte_index < PLAINSIGHT_CONTAINER_SALT_BYTES; byte_index++) {
        out[write_offset + byte_index] = outer->salt[byte_index];
    }
    write_offset += PLAINSIGHT_CONTAINER_SALT_BYTES;

    for (byte_index = 0u; byte_index < PLAINSIGHT_CONTAINER_NONCE_BYTES; byte_index++) {
        out[write_offset + byte_index] = outer->nonce[byte_index];
    }
    write_offset += PLAINSIGHT_CONTAINER_NONCE_BYTES;

    plainsight_write_u64_le(out + write_offset, (uint64_t)ciphertext_len);
    write_offset += 8u;

    for (byte_index = 0u; byte_index < ciphertext_len; byte_index++) {
        out[write_offset + byte_index] = ciphertext[byte_index];
    }
    write_offset += ciphertext_len;

    *out_len = write_offset;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_container_parse_outer(const uint8_t *in,
                                                  size_t in_len,
                                                  plainsight_outer_header *outer,
                                                  const uint8_t **ciphertext,
                                                  size_t *ciphertext_len) {
    plainsight_outer_header parsed_outer = {0};
    size_t byte_index = 0u;
    size_t read_offset = 0u;
    uint64_t encoded_len = 0u;

    if (in == NULL || outer == NULL || ciphertext == NULL || ciphertext_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (in_len < PLAINSIGHT_CONTAINER_OUTER_FIXED_BYTES) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    for (byte_index = 0u; byte_index < PLAINSIGHT_CONTAINER_MAGIC_LEN; byte_index++) {
        if (in[read_offset + byte_index] != PLAINSIGHT_CONTAINER_MAGIC[byte_index]) {
            return PLAINSIGHT_ERR_BAD_FORMAT;
        }
    }
    read_offset += PLAINSIGHT_CONTAINER_MAGIC_LEN;

    parsed_outer.version = in[read_offset];
    read_offset += 1u;

    if (parsed_outer.version != PLAINSIGHT_CONTAINER_VERSION) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    if (in[read_offset] != 0u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    read_offset += 1u;

    parsed_outer.kdf_alg = plainsight_read_u16_le(in + read_offset);
    read_offset += 2u;

    parsed_outer.kdf_opslimit = plainsight_read_u64_le(in + read_offset);
    read_offset += 8u;

    parsed_outer.kdf_memlimit = plainsight_read_u64_le(in + read_offset);
    read_offset += 8u;

    for (byte_index = 0u; byte_index < PLAINSIGHT_CONTAINER_SALT_BYTES; byte_index++) {
        parsed_outer.salt[byte_index] = in[read_offset + byte_index];
    }
    read_offset += PLAINSIGHT_CONTAINER_SALT_BYTES;

    for (byte_index = 0u; byte_index < PLAINSIGHT_CONTAINER_NONCE_BYTES; byte_index++) {
        parsed_outer.nonce[byte_index] = in[read_offset + byte_index];
    }
    read_offset += PLAINSIGHT_CONTAINER_NONCE_BYTES;

    encoded_len = plainsight_read_u64_le(in + read_offset);
    read_offset += 8u;

    if (encoded_len < PLAINSIGHT_CONTAINER_AEAD_TAG_BYTES ||
        encoded_len > PLAINSIGHT_MAX_CIPHERTEXT_BYTES ||
        encoded_len > (uint64_t)SIZE_MAX ||
        encoded_len > (uint64_t)(in_len - read_offset)) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    if ((uint64_t)read_offset + encoded_len != (uint64_t)in_len) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    parsed_outer.ciphertext_len = encoded_len;

    *outer = parsed_outer;
    *ciphertext = in + read_offset;
    *ciphertext_len = (size_t)encoded_len;

    return PLAINSIGHT_OK;
}
