// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>

#include "../../include/split/manifest.h"

// The manifest is an encrypted header stored inside shard 0 plaintext
// It describes how to reassemble the original payload across shards

static const uint8_t PLAINSIGHT_SPLIT_MANIFEST_MAGIC[4] = {
    (uint8_t)'H', (uint8_t)'I', (uint8_t)'S', (uint8_t)'M'
};

static void plainsight_split_manifest_write_u16_le(uint8_t *out, uint16_t value) {
    // Fixed-width writes keep the format stable across compilers and CPUs
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8u) & 0xFFu);
}

static void plainsight_split_manifest_write_u32_le(uint8_t *out, uint32_t value) {
    // Avoids unaligned casts and endianness assumptions
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8u) & 0xFFu);
    out[2] = (uint8_t)((value >> 16u) & 0xFFu);
    out[3] = (uint8_t)((value >> 24u) & 0xFFu);
}

static void plainsight_split_manifest_write_u64_le(uint8_t *out, uint64_t value) {
    size_t byte_index = 0u;
    // Loop keeps the byte order explicit
    for (byte_index = 0u; byte_index < 8u; byte_index++) {
        out[byte_index] = (uint8_t)((value >> (8u * byte_index)) & 0xFFu);
    }
}

static uint32_t plainsight_split_manifest_read_u32_le(const uint8_t *in) {
    uint32_t value = 0u;
    // Reads mirror writes for symmetry and auditability
    value |= (uint32_t)in[0];
    value |= ((uint32_t)in[1] << 8u);
    value |= ((uint32_t)in[2] << 16u);
    value |= ((uint32_t)in[3] << 24u);
    return value;
}

static uint64_t plainsight_split_manifest_read_u64_le(const uint8_t *in) {
    uint64_t value = 0u;
    size_t byte_index = 0u;
    // Loop form keeps the little-endian decode obvious
    for (byte_index = 0u; byte_index < 8u; byte_index++) {
        value |= ((uint64_t)in[byte_index]) << (8u * byte_index);
    }
    return value;
}

static plainsight_error plainsight_split_manifest_expected_len(uint32_t shard_count,
                                               uint8_t flags,
                                               size_t *expected_len_out) {
    size_t expected_len = 0u;

    if (expected_len_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (shard_count == 0u || shard_count > PLAINSIGHT_MAX_SHARDS) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Base prefix is fixed, tables scale with shard_count
    expected_len = 44u + ((size_t)shard_count * 4u);
    if ((flags & PLAINSIGHT_SPLIT_MANIFEST_FLAG_HAS_CIPHER_LEN) != 0u) {
        if ((size_t)shard_count > (SIZE_MAX - expected_len) / 8u) {
            return PLAINSIGHT_ERR_BAD_FORMAT;
        }
        expected_len += (size_t)shard_count * 8u;
    }

    if (expected_len > PLAINSIGHT_SPLIT_MANIFEST_MAX_BYTES) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    *expected_len_out = expected_len;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_split_manifest_pack(uint8_t manifest_version,
                                uint8_t flags,
                                const uint8_t set_id[PLAINSIGHT_SPLIT_SET_ID_BYTES],
                                uint64_t total_plaintext_len,
                                uint8_t compression_mode,
                                uint32_t chunk_plain_len,
                                uint32_t shard_count,
                                const uint32_t *per_shard_plain_len,
                                const uint64_t *per_shard_cipher_len,
                                uint8_t *out,
                                size_t out_cap,
                                size_t *out_len) {
    size_t write_offset = 0u;
    uint64_t sum_plain_len = 0u;
    size_t shard_index = 0u;
    size_t expected_len = 0u;

    if (set_id == NULL || per_shard_plain_len == NULL || out == NULL || out_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (manifest_version != PLAINSIGHT_SPLIT_MANIFEST_VERSION) {
        // Unknown versions are rejected so callers do not guess field layouts
        return PLAINSIGHT_ERR_ARGS;
    }
    if ((flags & (uint8_t)(~PLAINSIGHT_SPLIT_MANIFEST_FLAGS_KNOWN)) != 0u) {
        // Fail-closed for forward compatibility
        return PLAINSIGHT_ERR_ARGS;
    }
    if (shard_count == 0u || shard_count > PLAINSIGHT_MAX_SHARDS) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (chunk_plain_len == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (total_plaintext_len > PLAINSIGHT_MAX_TOTAL_PAYLOAD_BYTES) {
        // Total payload is bounded so extraction cannot be forced to write unbounded output
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    if ((flags & PLAINSIGHT_SPLIT_MANIFEST_FLAG_HAS_CIPHER_LEN) != 0u && per_shard_cipher_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // expected_len is computed up front so writers can preflight buffer sizing
    if (plainsight_split_manifest_expected_len(shard_count, flags, &expected_len) != PLAINSIGHT_OK) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    if (expected_len > PLAINSIGHT_SPLIT_MANIFEST_MAX_BYTES || expected_len > out_cap) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    // Sum check ensures per-shard table matches total plaintext length exactly
    // This prevents silent truncation or length mismatches during assembly
    for (shard_index = 0u; shard_index < (size_t)shard_count; shard_index++) {
        if (sum_plain_len > UINT64_MAX - (uint64_t)per_shard_plain_len[shard_index]) {
            return PLAINSIGHT_ERR_TOO_LARGE;
        }
        sum_plain_len += (uint64_t)per_shard_plain_len[shard_index];
    }
    if (sum_plain_len != total_plaintext_len) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Magic + version is the minimum needed to identify and validate this prefix
    out[write_offset++] = PLAINSIGHT_SPLIT_MANIFEST_MAGIC[0];
    out[write_offset++] = PLAINSIGHT_SPLIT_MANIFEST_MAGIC[1];
    out[write_offset++] = PLAINSIGHT_SPLIT_MANIFEST_MAGIC[2];
    out[write_offset++] = PLAINSIGHT_SPLIT_MANIFEST_MAGIC[3];
    out[write_offset++] = manifest_version;
    out[write_offset++] = flags;
    // Reserved bytes are written as zero and must be checked on parse
    plainsight_split_manifest_write_u16_le(out + write_offset, 0u);
    write_offset += 2u;

    // set_id is duplicated inside the encrypted manifest for integrity cross-checks
    for (shard_index = 0u; shard_index < PLAINSIGHT_SPLIT_SET_ID_BYTES; shard_index++) {
        out[write_offset + shard_index] = set_id[shard_index];
    }
    write_offset += PLAINSIGHT_SPLIT_SET_ID_BYTES;
    plainsight_split_manifest_write_u64_le(out + write_offset, total_plaintext_len);
    write_offset += 8u;
    // compression_mode is reserved for phase 3, stored so future builds can evolve safely
    out[write_offset++] = compression_mode;
    out[write_offset++] = 0u;
    plainsight_split_manifest_write_u16_le(out + write_offset, 0u);
    write_offset += 2u;
    plainsight_split_manifest_write_u32_le(out + write_offset, chunk_plain_len);
    write_offset += 4u;
    plainsight_split_manifest_write_u32_le(out + write_offset, shard_count);
    write_offset += 4u;

    // Per-shard plaintext lengths describe only payload data bytes, not the manifest prefix
    for (shard_index = 0u; shard_index < (size_t)shard_count; shard_index++) {
        plainsight_split_manifest_write_u32_le(out + write_offset, per_shard_plain_len[shard_index]);
        write_offset += 4u;
    }

    if ((flags & PLAINSIGHT_SPLIT_MANIFEST_FLAG_HAS_CIPHER_LEN) != 0u) {
        // Optional ciphertext lengths allow strict cross-checking with outer headers
        for (shard_index = 0u; shard_index < (size_t)shard_count; shard_index++) {
            plainsight_split_manifest_write_u64_le(out + write_offset, per_shard_cipher_len[shard_index]);
            write_offset += 8u;
        }
    }

    *out_len = write_offset;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_split_manifest_parse(const uint8_t *in,
                                 size_t in_len,
                                 plainsight_split_manifest_view *manifest_view) {
    size_t read_offset = 0u;
    size_t shard_index = 0u;
    uint64_t sum_plain_len = 0u;
    size_t expected_len = 0u;
    const uint8_t *plain_table_bytes = NULL;
    const uint8_t *cipher_table_bytes = NULL;

    if (in == NULL || manifest_view == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (in_len < 44u || in_len > PLAINSIGHT_SPLIT_MANIFEST_MAX_BYTES) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Magic verifies this is a manifest prefix and not random plaintext
    if (in[0] != PLAINSIGHT_SPLIT_MANIFEST_MAGIC[0] ||
        in[1] != PLAINSIGHT_SPLIT_MANIFEST_MAGIC[1] ||
        in[2] != PLAINSIGHT_SPLIT_MANIFEST_MAGIC[2] ||
        in[3] != PLAINSIGHT_SPLIT_MANIFEST_MAGIC[3]) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    read_offset += 4u;

    manifest_view->manifest_version = in[read_offset++];
    manifest_view->flags = in[read_offset++];
    if (manifest_view->manifest_version != PLAINSIGHT_SPLIT_MANIFEST_VERSION) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    if ((manifest_view->flags & (uint8_t)(~PLAINSIGHT_SPLIT_MANIFEST_FLAGS_KNOWN)) != 0u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    // Reserved bytes must be zero so older builds do not misread future extensions
    if (in[read_offset] != 0u || in[read_offset + 1u] != 0u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    read_offset += 2u;

    for (shard_index = 0u; shard_index < PLAINSIGHT_SPLIT_SET_ID_BYTES; shard_index++) {
        manifest_view->set_id[shard_index] = in[read_offset + shard_index];
    }
    read_offset += PLAINSIGHT_SPLIT_SET_ID_BYTES;
    manifest_view->total_plaintext_len = plainsight_split_manifest_read_u64_le(in + read_offset);
    read_offset += 8u;
    manifest_view->compression_mode = in[read_offset++];
    // Reserved bytes keep alignment room for future fields
    if (in[read_offset] != 0u || in[read_offset + 1u] != 0u || in[read_offset + 2u] != 0u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    read_offset += 3u;
    manifest_view->chunk_plain_len = plainsight_split_manifest_read_u32_le(in + read_offset);
    read_offset += 4u;
    manifest_view->shard_count = plainsight_split_manifest_read_u32_le(in + read_offset);
    read_offset += 4u;

    if (manifest_view->shard_count == 0u || manifest_view->shard_count > PLAINSIGHT_MAX_SHARDS) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    if (manifest_view->chunk_plain_len == 0u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    if (manifest_view->total_plaintext_len > PLAINSIGHT_MAX_TOTAL_PAYLOAD_BYTES) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // expected_len is strict so trailing bytes inside the manifest prefix are rejected
    if (plainsight_split_manifest_expected_len(manifest_view->shard_count, manifest_view->flags, &expected_len) != PLAINSIGHT_OK) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    if (expected_len != in_len) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    plain_table_bytes = in + read_offset;
    // Sum validation prevents a payload assembly loop from writing beyond total length
    for (shard_index = 0u; shard_index < (size_t)manifest_view->shard_count; shard_index++) {
        uint32_t plain_len = plainsight_split_manifest_read_u32_le(plain_table_bytes + (shard_index * 4u));
        if (sum_plain_len > UINT64_MAX - (uint64_t)plain_len) {
            return PLAINSIGHT_ERR_BAD_FORMAT;
        }
        sum_plain_len += (uint64_t)plain_len;
    }
    if (sum_plain_len != manifest_view->total_plaintext_len) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    read_offset += (size_t)manifest_view->shard_count * 4u;

    if ((manifest_view->flags & PLAINSIGHT_SPLIT_MANIFEST_FLAG_HAS_CIPHER_LEN) != 0u) {
        cipher_table_bytes = in + read_offset;
    }

    // Views point into the original plaintext buffer to avoid extra copies
    manifest_view->per_shard_plain_len_bytes = plain_table_bytes;
    manifest_view->per_shard_cipher_len_bytes = cipher_table_bytes;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_split_manifest_parse_prefix(const uint8_t *in,
                                        size_t in_len,
                                        plainsight_split_manifest_view *manifest_view,
                                        size_t *out_len) {
    size_t expected_len = 0u;
    uint32_t shard_count = 0u;
    uint8_t flags = 0u;

    if (in == NULL || manifest_view == NULL || out_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Minimum bytes needed to read shard_count and flags safely
    if (in_len < 44u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Read shard_count and flags without trusting the whole buffer yet
    // Layout is fixed for the first 44 bytes
    flags = in[5];
    shard_count = plainsight_split_manifest_read_u32_le(in + 40u);

    if (plainsight_split_manifest_expected_len(shard_count, flags, &expected_len) != PLAINSIGHT_OK) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    if (expected_len > in_len) {
        // Plaintext buffer is smaller than claimed manifest prefix
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Parse the exact prefix so the caller can treat remaining bytes as shard 0 data
    if (plainsight_split_manifest_parse(in, expected_len, manifest_view) != PLAINSIGHT_OK) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Caller uses out_len to locate the start of shard 0 data bytes inside plaintext
    *out_len = expected_len;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_split_manifest_plain_len_at(const plainsight_split_manifest_view *manifest_view,
                                        uint32_t shard_index,
                                        uint32_t *plain_len_out) {
    if (manifest_view == NULL || plain_len_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (manifest_view->per_shard_plain_len_bytes == NULL) {
        // This is a caller misuse or a parse failure earlier in the pipeline
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    if (shard_index >= manifest_view->shard_count) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    *plain_len_out = plainsight_split_manifest_read_u32_le(
        manifest_view->per_shard_plain_len_bytes + ((size_t)shard_index * 4u));
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_split_manifest_cipher_len_at(const plainsight_split_manifest_view *manifest_view,
                                         uint32_t shard_index,
                                         uint64_t *cipher_len_out) {
    if (manifest_view == NULL || cipher_len_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (manifest_view->per_shard_cipher_len_bytes == NULL) {
        // Cipher length table is optional and is present only when flagged
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    if (shard_index >= manifest_view->shard_count) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    *cipher_len_out = plainsight_split_manifest_read_u64_le(
        manifest_view->per_shard_cipher_len_bytes + ((size_t)shard_index * 8u));
    return PLAINSIGHT_OK;
}
