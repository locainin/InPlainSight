#ifndef PLAINSIGHT_CONTAINER_H
#define PLAINSIGHT_CONTAINER_H

#include <stddef.h>
#include <stdint.h>

#include "error.h"
#include "io.h"

#ifdef __cplusplus
extern "C" {
#endif

// Versioned container constants
#define PLAINSIGHT_CONTAINER_VERSION 1u
#define PLAINSIGHT_CONTAINER_MAGIC_LEN 8u
#define PLAINSIGHT_CONTAINER_OUTER_FIXED_BYTES 76u
#define PLAINSIGHT_CONTAINER_AEAD_TAG_BYTES 16u
#define PLAINSIGHT_MAX_INNER_BYTES (PLAINSIGHT_MAX_PAYLOAD_BYTES + 512u)
#define PLAINSIGHT_MAX_CIPHERTEXT_BYTES (PLAINSIGHT_MAX_INNER_BYTES + PLAINSIGHT_CONTAINER_AEAD_TAG_BYTES)
#define PLAINSIGHT_MAX_CONTAINER_BYTES (PLAINSIGHT_CONTAINER_OUTER_FIXED_BYTES + PLAINSIGHT_MAX_CIPHERTEXT_BYTES)

typedef struct plainsight_outer_header {
    uint8_t version;
    uint16_t kdf_alg;
    uint64_t kdf_opslimit;
    uint64_t kdf_memlimit;
    uint8_t salt[16];
    uint8_t nonce[24];
    uint64_t ciphertext_len;
} plainsight_outer_header;

// Inner metadata stays inside encryption so it is not visible in plain bytes
typedef struct plainsight_inner_header {
    uint8_t compression;
    const uint8_t *name;
    uint16_t name_len;
    const uint8_t *mime;
    uint16_t mime_len;
    const uint8_t *payload;
    uint64_t payload_len;
} plainsight_inner_header;

// Packs inner metadata and payload into one byte sequence
plainsight_error plainsight_container_pack_inner(const plainsight_inner_header *inner,
                                 uint8_t *out,
                                 size_t out_cap,
                                 size_t *out_len);

// Parses decrypted inner bytes into views over the same buffer
plainsight_error plainsight_container_parse_inner(const uint8_t *in,
                                  size_t in_len,
                                  plainsight_inner_header *inner_view);

// Packs outer public header and ciphertext
plainsight_error plainsight_container_pack_outer(const plainsight_outer_header *outer,
                                 const uint8_t *ciphertext,
                                 size_t ciphertext_len,
                                 uint8_t *out,
                                 size_t out_cap,
                                 size_t *out_len);

// Parses outer header and returns a pointer to ciphertext bytes
plainsight_error plainsight_container_parse_outer(const uint8_t *in,
                                  size_t in_len,
                                  plainsight_outer_header *outer,
                                  const uint8_t **ciphertext,
                                  size_t *ciphertext_len);

#ifdef __cplusplus
}
#endif

#endif
