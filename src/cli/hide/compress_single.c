// InPlainSight C module
// Single-image auto compression tests bounded candidates and keeps only smaller output

#include <stddef.h>
#include <stdint.h>

#include "headers/single.h"

static void plainsight_cli_hide_copy_bytes(uint8_t *destination, size_t destination_length,
                                           const uint8_t *source, size_t source_length) {
  size_t index = 0u;

  if (destination == NULL || source == NULL || source_length > destination_length) {
    return;
  }

  // Manual copy avoids unbounded C library calls and keeps the exact byte count visible
  for (index = 0u; index < source_length; index++) {
    destination[index] = source[index];
  }
}

plainsight_error plainsight_cli_hide_choose_auto_compression(size_t payload_length,
                                                             size_t *compressed_length_out) {
  static const int zstd_auto_levels[] = {1, 3, 9};
  size_t best_length = 0u;
  size_t level_index = 0u;
  int found_candidate = 0;

  if (compressed_length_out == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }
  *compressed_length_out = 0u;

  for (level_index = 0u; level_index < (sizeof(zstd_auto_levels) / sizeof(zstd_auto_levels[0]));
       level_index++) {
    size_t candidate_length = 0u;
    const plainsight_error result_code = plainsight_compress_zstd_level(
        g_cli_workspace.payload, payload_length, zstd_auto_levels[level_index], g_cli_workspace.inner,
        sizeof(g_cli_workspace.inner), &candidate_length);
    if (result_code != PLAINSIGHT_OK) {
      continue;
    }
    if (candidate_length >= payload_length) {
      continue;
    }
    if (found_candidate == 0 || candidate_length < best_length) {
      if (candidate_length > sizeof(g_cli_workspace.ciphertext)) {
        return PLAINSIGHT_ERR_TOO_LARGE;
      }
      plainsight_cli_hide_copy_bytes(g_cli_workspace.ciphertext, sizeof(g_cli_workspace.ciphertext),
                                     g_cli_workspace.inner, candidate_length);
      best_length = candidate_length;
      found_candidate = 1;
    }
  }

  if (found_candidate == 0) {
    return PLAINSIGHT_ERR_UNSUPPORTED;
  }

  *compressed_length_out = best_length;
  return PLAINSIGHT_OK;
}
