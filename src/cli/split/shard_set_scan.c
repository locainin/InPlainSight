// InPlainSight C module
// Directory scanning is isolated from decryption so set selection stays easy to audit

#include <stddef.h>
#include <stdint.h>

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "headers/shard_set.h"

#include "../../../include/split/collect.h"

static int plainsight_cli_extension_is_supported_input(const char *name_text) {
  plainsight_image_format format = plainsight_image_detect_format_from_path(name_text);
  return format != PLAINSIGHT_IMAGE_FORMAT_UNKNOWN;
}

static plainsight_error plainsight_cli_copy_name(const char *name_text, char *out, size_t out_cap) {
  size_t index = 0u;

  if (name_text == NULL || out == NULL || out_cap == 0u) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // Filename strings are only used for reopening the file later
  // The header fields drive ordering and validation
  while (name_text[index] != '\0') {
    if (index + 1u >= out_cap) {
      return PLAINSIGHT_ERR_TOO_LARGE;
    }
    out[index] = name_text[index];
    index++;
  }
  out[index] = '\0';
  return PLAINSIGHT_OK;
}

static void plainsight_cli_split_scan_reset(plainsight_cli_split_shard_set *set_out, uint8_t *shard_present) {
  for (uint32_t index = 0u; index < PLAINSIGHT_MAX_SHARDS; index++) {
    shard_present[index] = 0u;
    set_out->shard_name_by_index[index][0] = '\0';
    plainsight_secure_zero(&set_out->shard_headers[index], sizeof(set_out->shard_headers[index]));
  }
  set_out->shard_count = 0u;
  plainsight_secure_zero(set_out->expected_set_id, sizeof(set_out->expected_set_id));
}

static int plainsight_cli_outer_matches_set(const plainsight_split_outer_v2 *outer,
                                            const uint8_t expected_set_id[PLAINSIGHT_SPLIT_SET_ID_BYTES]) {
  for (uint32_t index = 0u; index < PLAINSIGHT_SPLIT_SET_ID_BYTES; index++) {
    if (outer->set_id[index] != expected_set_id[index]) {
      return 0;
    }
  }
  return 1;
}

static void plainsight_cli_copy_set_id(uint8_t out[PLAINSIGHT_SPLIT_SET_ID_BYTES],
                                       const uint8_t in[PLAINSIGHT_SPLIT_SET_ID_BYTES]) {
  for (uint32_t index = 0u; index < PLAINSIGHT_SPLIT_SET_ID_BYTES; index++) {
    out[index] = in[index];
  }
}

plainsight_error plainsight_cli_split_scan_directory(
    const char *input_dir, const char *expanded_input_dir, plainsight_embed_method method,
    const uint8_t stego_subkey[PLAINSIGHT_STEGO_SUBKEY_BYTES], plainsight_cli_split_shard_set *set_out) {
  DIR *directory_handle = NULL;
  const struct dirent *entry = NULL;
  uint8_t shard_present[PLAINSIGHT_MAX_SHARDS];
  char path_buffer[1024];
  plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
  size_t scanned_entries = 0u;
  int have_set = 0;
  int saw_other_set = 0;

  if (input_dir == NULL || expanded_input_dir == NULL || stego_subkey == NULL || set_out == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  plainsight_cli_split_scan_reset(set_out, shard_present);

  // opendir does not expand shell-style home paths, so use the validated real path
  directory_handle = opendir(expanded_input_dir);
  if (directory_handle == NULL) {
    return PLAINSIGHT_ERR_IO;
  }

  while ((entry = readdir(directory_handle)) != NULL) {
    plainsight_split_outer_v2 outer;
    const uint8_t *ciphertext = NULL;
    size_t ciphertext_len = 0u;
    size_t container_len = 0u;

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    if (!plainsight_cli_extension_is_supported_input(entry->d_name)) {
      continue;
    }

    // Cap scan work for untrusted directories
    // This prevents a directory with many files from consuming unbounded time
    scanned_entries++;
    if (scanned_entries > (size_t)PLAINSIGHT_MAX_SET_SCAN_IMAGES) {
      result_code = PLAINSIGHT_ERR_AUTH;
      goto cleanup;
    }

    if (plainsight_cli_join_dir_and_name(input_dir, entry->d_name, path_buffer, sizeof(path_buffer)) !=
        PLAINSIGHT_OK) {
      continue;
    }

    result_code = plainsight_cli_split_read_shard(
        path_buffer, method, stego_subkey, g_cli_workspace.container, sizeof(g_cli_workspace.container),
        &container_len, &outer, &ciphertext, &ciphertext_len);
    if (result_code != PLAINSIGHT_OK) {
      continue;
    }

    if (outer.shard_count == 0u || outer.shard_count > PLAINSIGHT_MAX_SHARDS) {
      continue;
    }
    if (outer.shard_index >= outer.shard_count) {
      continue;
    }
    // Shard 0 must carry a manifest prefix inside plaintext
    if (outer.shard_index == 0u && (outer.flags & PLAINSIGHT_SPLIT_FLAG_HAS_MANIFEST) == 0u) {
      continue;
    }

    if (have_set == 0) {
      set_out->shard_count = outer.shard_count;
      plainsight_cli_copy_set_id(set_out->expected_set_id, outer.set_id);
      have_set = 1;
    } else if (outer.shard_count != set_out->shard_count) {
      continue;
    } else if (plainsight_cli_outer_matches_set(&outer, set_out->expected_set_id) == 0) {
      // A second distinct set_id means multiple payload sets exist in this directory
      // Default behavior is fail closed to avoid accidental mixing
      saw_other_set = 1;
      continue;
    }

    if (shard_present[outer.shard_index] != 0u) {
      result_code = PLAINSIGHT_ERR_AUTH;
      goto cleanup;
    }
    if (plainsight_cli_copy_name(entry->d_name, set_out->shard_name_by_index[outer.shard_index],
                                 sizeof(set_out->shard_name_by_index[0])) != PLAINSIGHT_OK) {
      result_code = PLAINSIGHT_ERR_IO;
      goto cleanup;
    }

    shard_present[outer.shard_index] = 1u;
    set_out->shard_headers[outer.shard_index] = outer;
  }

  if (have_set == 0 || saw_other_set != 0) {
    result_code = PLAINSIGHT_ERR_AUTH;
    goto cleanup;
  }

  // The shared validator owns duplicate, missing index, and set consistency rules
  result_code = plainsight_split_collect_validate_set(set_out->shard_headers, (size_t)set_out->shard_count,
                                                      set_out->expected_set_id, &set_out->shard_count);
  if (result_code != PLAINSIGHT_OK) {
    result_code = PLAINSIGHT_ERR_AUTH;
    goto cleanup;
  }

  result_code = PLAINSIGHT_OK;

cleanup:
  if (directory_handle != NULL) {
    (void)closedir(directory_handle);
  }
  plainsight_secure_zero(g_cli_workspace.container, sizeof(g_cli_workspace.container));
  return result_code;
}
