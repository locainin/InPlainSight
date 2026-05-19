#ifndef PLAINSIGHT_SPLIT_MANIFEST_H
#define PLAINSIGHT_SPLIT_MANIFEST_H

#include <stddef.h>
#include <stdint.h>

#include "../error.h"
#include "outer_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

// Versioned split manifest with strict bounds for encrypted shard-0 payload
#define PLAINSIGHT_SPLIT_MANIFEST_VERSION 1u
#define PLAINSIGHT_SPLIT_MANIFEST_MAX_BYTES (64ULL * 1024ULL)

#define PLAINSIGHT_SPLIT_MANIFEST_FLAG_HAS_CIPHER_LEN 0x01u
#define PLAINSIGHT_SPLIT_MANIFEST_FLAGS_KNOWN PLAINSIGHT_SPLIT_MANIFEST_FLAG_HAS_CIPHER_LEN

typedef struct plainsight_split_manifest_view {
  // Manifest version for fail-closed parsing
  uint8_t manifest_version;
  // Feature flags for optional tables
  uint8_t flags;
  // set_id duplicated inside ciphertext for integrity checks
  uint8_t set_id[PLAINSIGHT_SPLIT_SET_ID_BYTES];
  // Total payload bytes across all shards, excluding manifest bytes
  uint64_t total_plaintext_len;
  // Compression mode for future support, currently reserved
  uint8_t compression_mode;
  // Nominal chunk size used by shards after shard 0
  uint32_t chunk_plain_len;
  // Total shard count for this set
  uint32_t shard_count;
  // In-buffer view of the per-shard plaintext length table
  const uint8_t *per_shard_plain_len_bytes;
  // Optional in-buffer view of the per-shard ciphertext length table
  const uint8_t *per_shard_cipher_len_bytes;
} plainsight_split_manifest_view;

// Pack input keeps the manifest wire fields grouped and named
typedef struct plainsight_split_manifest_pack_input {
  // Manifest format version, currently PLAINSIGHT_SPLIT_MANIFEST_VERSION
  uint8_t manifest_version;
  // Feature flags describing optional tables
  uint8_t flags;
  // Random set id shared by all shards in one split payload
  const uint8_t *set_id;
  // Total payload bytes before split wrapping
  uint64_t total_plaintext_len;
  // Compression marker reserved for compatible future support
  uint8_t compression_mode;
  // Nominal per-shard plaintext chunk size
  uint32_t chunk_plain_len;
  // Number of shards in this set
  uint32_t shard_count;
  // One plaintext length per shard
  const uint32_t *per_shard_plain_len;
  // Optional one ciphertext length per shard
  const uint64_t *per_shard_cipher_len;
} plainsight_split_manifest_pack_input;

// Packs split manifest with caller-provided per-shard length arrays
plainsight_error plainsight_split_manifest_pack(const plainsight_split_manifest_pack_input *input,
                                                uint8_t *out, size_t out_cap, size_t *out_len);

// Parses split manifest into in-buffer array views with strict checks
// This parser expects the buffer length to match the exact manifest prefix length
plainsight_error plainsight_split_manifest_parse(const uint8_t *in, size_t in_len,
                                                 plainsight_split_manifest_view *manifest_view);

// Parses a manifest prefix from a larger plaintext buffer
// On success, out_len is set to the exact manifest prefix length in bytes
plainsight_error plainsight_split_manifest_parse_prefix(const uint8_t *in, size_t in_len,
                                                        plainsight_split_manifest_view *manifest_view,
                                                        size_t *out_len);

// Reads one per-shard plaintext length entry from a parsed manifest view
plainsight_error plainsight_split_manifest_plain_len_at(const plainsight_split_manifest_view *manifest_view,
                                                        uint32_t shard_index, uint32_t *plain_len_out);

// Reads one per-shard ciphertext length entry when that table is present
plainsight_error plainsight_split_manifest_cipher_len_at(const plainsight_split_manifest_view *manifest_view,
                                                         uint32_t shard_index, uint64_t *cipher_len_out);

#ifdef __cplusplus
}
#endif

#endif
