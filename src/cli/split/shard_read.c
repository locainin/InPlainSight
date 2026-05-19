// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>

#include "headers/shard_set.h"

plainsight_error plainsight_cli_split_read_shard(const char *path, plainsight_embed_method method,
                                                 const uint8_t stego_subkey[PLAINSIGHT_STEGO_SUBKEY_BYTES],
                                                 uint8_t *container_out, size_t container_cap,
                                                 size_t *container_len_out,
                                                 plainsight_split_outer_v2 *outer_out,
                                                 const uint8_t **ciphertext_out, size_t *ciphertext_len_out) {
  plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
  uint8_t embed_seed[32];
  size_t container_len = 0u;

  if (path == NULL || stego_subkey == NULL || container_out == NULL || container_len_out == NULL ||
      outer_out == NULL || ciphertext_out == NULL || ciphertext_len_out == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // Each candidate must be decoded so pixels can be inspected by the embed backend
  // This is the cost of a keyed pixel stego scheme
  result_code = plainsight_cli_load_image(path, &g_cli_workspace.image);
  if (result_code != PLAINSIGHT_OK) {
    return PLAINSIGHT_ERR_AUTH;
  }

  // Seed derivation masks out LSBs so it stays stable between cover and stego outputs
  // This allows outer header extraction without knowing the encryption salt in advance
  result_code = plainsight_crypto_seed_from_subkey_and_cover(stego_subkey, g_cli_workspace.image.pixels,
                                                             g_cli_workspace.image.data_len, embed_seed);
  if (result_code != PLAINSIGHT_OK) {
    plainsight_secure_zero(embed_seed, sizeof(embed_seed));
    return PLAINSIGHT_ERR_AUTH;
  }

  // Only the embedded bytes are returned here
  // The outer header parse later slices ciphertext without copying
  result_code = plainsight_extract_payload(method, g_cli_workspace.image.pixels,
                                           g_cli_workspace.image.data_len, g_cli_workspace.image.channels,
                                           container_out, container_cap, &container_len, embed_seed);
  plainsight_secure_zero(embed_seed, sizeof(embed_seed));
  if (result_code != PLAINSIGHT_OK) {
    return PLAINSIGHT_ERR_AUTH;
  }

  // Parse outer header strictly to reject unknown versions and flags early
  result_code = plainsight_split_outer_v2_parse(container_out, container_len, outer_out, ciphertext_out,
                                                ciphertext_len_out);
  if (result_code != PLAINSIGHT_OK) {
    return PLAINSIGHT_ERR_AUTH;
  }

  *container_len_out = container_len;
  return PLAINSIGHT_OK;
}
