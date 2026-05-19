#ifndef PLAINSIGHT_SPLIT_OUTER_V2_H
#define PLAINSIGHT_SPLIT_OUTER_V2_H

#include <stddef.h>
#include <stdint.h>

#include "../container.h"
#include "../error.h"

#ifdef __cplusplus
extern "C" {
#endif

// Split-capable container version and public header sizing
#define PLAINSIGHT_SPLIT_OUTER_VERSION 2u
#define PLAINSIGHT_SPLIT_SET_ID_BYTES 16u
#define PLAINSIGHT_SPLIT_OUTER_FIXED_BYTES 100u

// Split flags are public metadata and must be authenticated via AAD
#define PLAINSIGHT_SPLIT_FLAG_HAS_MANIFEST 0x01u
#define PLAINSIGHT_SPLIT_FLAGS_KNOWN PLAINSIGHT_SPLIT_FLAG_HAS_MANIFEST

// Split caps keep directory scans and shard sets bounded
#define PLAINSIGHT_MAX_SHARDS 1024u
#define PLAINSIGHT_MAX_TOTAL_PAYLOAD_BYTES (512ULL * 1024ULL * 1024ULL)
#define PLAINSIGHT_MAX_SET_SCAN_IMAGES 4096u

// Outer header for split shard containers
typedef struct plainsight_split_outer_v2 {
  // Format version for the split header layout
  uint8_t version;
  // Feature flags for forward compatibility
  uint8_t flags;
  // crypto_pwhash algorithm id stored for key derivation
  uint16_t kdf_alg;
  // KDF ops limit stored as a public parameter
  uint64_t kdf_opslimit;
  // KDF memory limit stored as a public parameter
  uint64_t kdf_memlimit;
  // Random salt used by Argon2id KDF
  uint8_t salt[16];
  // Random AEAD nonce for the shard ciphertext
  uint8_t nonce[24];
  // Random per-set identifier used to prevent mixing shards between runs
  uint8_t set_id[PLAINSIGHT_SPLIT_SET_ID_BYTES];
  // Zero-based shard index within the set
  uint32_t shard_index;
  // Total shard count for this set
  uint32_t shard_count;
  // Ciphertext length in bytes, including AEAD tag
  uint64_t ciphertext_len;
} plainsight_split_outer_v2;

// Serializes canonical v2 outer bytes used as AEAD AAD
// The AAD bytes must be identical on both encrypt and decrypt
plainsight_error plainsight_split_aad_serialize_outer_v2(const plainsight_split_outer_v2 *outer, uint8_t *out,
                                                         size_t out_cap, size_t *out_len);

// Packs v2 outer bytes followed by ciphertext
plainsight_error plainsight_split_outer_v2_pack(const plainsight_split_outer_v2 *outer,
                                                const uint8_t *ciphertext, size_t ciphertext_len,
                                                uint8_t *out, size_t out_cap, size_t *out_len);

// Parses v2 outer bytes and returns pointer view into ciphertext
// The input length must match header + ciphertext_len exactly
plainsight_error plainsight_split_outer_v2_parse(const uint8_t *in, size_t in_len,
                                                 plainsight_split_outer_v2 *outer, const uint8_t **ciphertext,
                                                 size_t *ciphertext_len);

#ifdef __cplusplus
}
#endif

#endif
