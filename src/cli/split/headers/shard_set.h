#ifndef PLAINSIGHT_CLI_SPLIT_HEADERS_SHARD_SET_H
#define PLAINSIGHT_CLI_SPLIT_HEADERS_SHARD_SET_H

#include <stddef.h>
#include <stdint.h>

#include "../../../../include/crypto.h"
#include "../../../../include/embed/embed.h"
#include "../../../../include/error.h"
#include "../../../../include/split/outer_v2.h"
#include "../../internal.h"

typedef struct plainsight_cli_split_shard_set {
  // All accepted shard headers must match this random set id
  uint8_t expected_set_id[PLAINSIGHT_SPLIT_SET_ID_BYTES];
  // Validated number of shards in the set
  uint32_t shard_count;
  // Names are kept by shard index so pass two can reopen files deterministically
  char shard_name_by_index[PLAINSIGHT_MAX_SHARDS][256];
  // Outer headers collected during scan before authenticated decryption
  plainsight_split_outer_v2 shard_headers[PLAINSIGHT_MAX_SHARDS];
} plainsight_cli_split_shard_set;

plainsight_error plainsight_cli_split_scan_directory(
    const char *input_dir, const char *expanded_input_dir, plainsight_embed_method method,
    const uint8_t stego_subkey[PLAINSIGHT_STEGO_SUBKEY_BYTES], plainsight_cli_split_shard_set *set_out);

plainsight_error plainsight_cli_split_read_shard(const char *path, plainsight_embed_method method,
                                                 const uint8_t stego_subkey[PLAINSIGHT_STEGO_SUBKEY_BYTES],
                                                 uint8_t *container_out, size_t container_cap,
                                                 size_t *container_len_out,
                                                 plainsight_split_outer_v2 *outer_out,
                                                 const uint8_t **ciphertext_out, size_t *ciphertext_len_out);

plainsight_error plainsight_cli_split_guess_payload_name(const uint8_t *payload_bytes, size_t payload_len,
                                                         char *out, size_t out_cap);

plainsight_error plainsight_cli_split_resolve_output_path(const char *requested_output_path,
                                                          const char *payload_file_name, char *out,
                                                          size_t out_cap);

plainsight_error plainsight_cli_split_assemble_payload(
    const plainsight_extract_options *options, const plainsight_cli_split_shard_set *split_set,
    size_t passphrase_length, const uint8_t stego_subkey[PLAINSIGHT_STEGO_SUBKEY_BYTES]);

#endif
