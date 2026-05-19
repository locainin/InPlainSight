// InPlainSight C module
// Split extraction output helpers stay separate from shard authentication logic

#include <stddef.h>
#include <stdint.h>

#include <sys/stat.h>

#include "headers/shard_set.h"

plainsight_error plainsight_cli_split_resolve_output_path(const char *requested_output_path,
                                                          const char *payload_file_name, char *out,
                                                          size_t out_cap) {
  struct stat output_metadata;
  size_t path_len = 0u;
  char expanded_output_path[PLAINSIGHT_MAX_PATH_BYTES];
  plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

  if (requested_output_path == NULL || payload_file_name == NULL || out == NULL || out_cap == 0u) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // Directory outputs may be entered as ~/..., so stat must see the expanded path
  result_code = plainsight_io_expand_home_path(requested_output_path, expanded_output_path,
                                               sizeof(expanded_output_path));
  if (result_code != PLAINSIGHT_OK) {
    return result_code;
  }

  if (stat(expanded_output_path, &output_metadata) == 0 && S_ISDIR(output_metadata.st_mode)) {
    return plainsight_cli_join_dir_and_name(expanded_output_path, payload_file_name, out, out_cap);
  }

  // Non-directory output is treated as an explicit final payload path
  while (requested_output_path[path_len] != '\0') {
    if (path_len + 1u >= out_cap) {
      return PLAINSIGHT_ERR_TOO_LARGE;
    }
    out[path_len] = requested_output_path[path_len];
    path_len++;
  }
  out[path_len] = '\0';
  return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_split_guess_payload_name(const uint8_t *payload_bytes, size_t payload_len,
                                                         char *out, size_t out_cap) {
  const char *name_text = "recovered_payload.bin";
  size_t name_len = 0u;

  if (out == NULL || out_cap == 0u) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // Only cheap magic bytes are used
  // The output name is a fallback, not an authentication decision
  if (payload_bytes != NULL && payload_len >= 5u && payload_bytes[0] == (uint8_t)'%' &&
      payload_bytes[1] == (uint8_t)'P' && payload_bytes[2] == (uint8_t)'D' &&
      payload_bytes[3] == (uint8_t)'F' && payload_bytes[4] == (uint8_t)'-') {
    name_text = "recovered_payload.pdf";
  } else if (payload_bytes != NULL && payload_len >= 8u && payload_bytes[0] == 0x89u &&
             payload_bytes[1] == (uint8_t)'P' && payload_bytes[2] == (uint8_t)'N' &&
             payload_bytes[3] == (uint8_t)'G' && payload_bytes[4] == 0x0Du && payload_bytes[5] == 0x0Au &&
             payload_bytes[6] == 0x1Au && payload_bytes[7] == 0x0Au) {
    name_text = "recovered_payload.png";
  } else if (payload_bytes != NULL && payload_len >= 3u && payload_bytes[0] == 0xFFu &&
             payload_bytes[1] == 0xD8u && payload_bytes[2] == 0xFFu) {
    name_text = "recovered_payload.jpg";
  } else if (payload_bytes != NULL && payload_len >= 4u && payload_bytes[0] == (uint8_t)'P' &&
             payload_bytes[1] == (uint8_t)'K' && payload_bytes[2] == 0x03u && payload_bytes[3] == 0x04u) {
    name_text = "recovered_payload.zip";
  }

  while (name_text[name_len] != '\0') {
    if (name_len + 1u >= out_cap) {
      return PLAINSIGHT_ERR_TOO_LARGE;
    }
    out[name_len] = name_text[name_len];
    name_len++;
  }
  out[name_len] = '\0';
  return PLAINSIGHT_OK;
}
