// InPlainSight C module
// Split extraction entrypoint coordinates validation, key setup, scan, and assembly

#include <stddef.h>

#include "../internal.h"
#include "../split/headers/shard_set.h"

plainsight_error plainsight_cli_run_extract_split(const plainsight_extract_options *options) {
  plainsight_cli_split_shard_set split_set;
  uint8_t stego_subkey[PLAINSIGHT_STEGO_SUBKEY_BYTES];
  char expanded_input_dir[PLAINSIGHT_MAX_PATH_BYTES];
  plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
  plainsight_error passphrase_lock_result = PLAINSIGHT_ERR_INTERNAL;
  int input_is_directory = 0;
  size_t passphrase_length = 0u;

  if (options == NULL || options->input_dir == NULL || options->output_path == NULL ||
      options->passphrase_path == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  plainsight_secure_zero(&split_set, sizeof(split_set));
  plainsight_secure_zero(stego_subkey, sizeof(stego_subkey));

  result_code = plainsight_cli_path_is_directory(options->input_dir, &input_is_directory);
  if (result_code != PLAINSIGHT_OK) {
    return result_code;
  }
  if (input_is_directory == 0) {
    return PLAINSIGHT_ERR_ARGS;
  }

  result_code =
      plainsight_io_expand_home_path(options->input_dir, expanded_input_dir, sizeof(expanded_input_dir));
  if (result_code != PLAINSIGHT_OK) {
    return result_code;
  }

  result_code = plainsight_io_read_passphrase_file(options->passphrase_path, g_cli_workspace.passphrase,
                                                   sizeof(g_cli_workspace.passphrase), &passphrase_length);
  if (result_code != PLAINSIGHT_OK) {
    return result_code;
  }
  passphrase_lock_result = plainsight_secure_lock(g_cli_workspace.passphrase, passphrase_length);

  // Split directories may contain many image candidates
  // Deriving this once avoids one Argon2id call per scanned file
  result_code =
      plainsight_crypto_derive_stego_subkey(g_cli_workspace.passphrase, passphrase_length, stego_subkey);
  if (result_code != PLAINSIGHT_OK) {
    result_code = PLAINSIGHT_ERR_AUTH;
    goto cleanup;
  }

  result_code = plainsight_cli_split_scan_directory(options->input_dir, expanded_input_dir, options->method,
                                                    stego_subkey, &split_set);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  result_code = plainsight_cli_split_assemble_payload(options, &split_set, passphrase_length, stego_subkey);

cleanup:
  plainsight_secure_zero(stego_subkey, sizeof(stego_subkey));
  plainsight_secure_zero(&split_set, sizeof(split_set));
  plainsight_secure_zero(g_cli_workspace.inner, sizeof(g_cli_workspace.inner));
  plainsight_secure_zero(g_cli_workspace.container, sizeof(g_cli_workspace.container));

  if (passphrase_lock_result == PLAINSIGHT_OK) {
    (void)plainsight_secure_unlock(g_cli_workspace.passphrase, passphrase_length);
  } else {
    plainsight_secure_zero(g_cli_workspace.passphrase, passphrase_length);
  }

  return result_code;
}
