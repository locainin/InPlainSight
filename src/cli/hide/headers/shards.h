#ifndef PLAINSIGHT_CLI_HIDE_HEADERS_SHARDS_H
#define PLAINSIGHT_CLI_HIDE_HEADERS_SHARDS_H

#include <stddef.h>
#include <stdint.h>

#include "../../internal.h"
#include "../../../../include/split/outer_v2.h"
#include "../../../../include/split/plan.h"

// Context object keeps shard writer arguments explicit without a long positional call
// All pointed-to buffers are owned by write_shards.c and stay valid for the full shard loop
typedef struct plainsight_cli_hide_shard_write_context {
  // Parsed command options shared by every shard write
  const plainsight_hide_options *options;
  // Split plan already checked against the selected cover capacity
  const plainsight_split_plan *split_plan;
  // KDF parameters copied into each authenticated shard header
  const plainsight_kdf_params *kdf_params;
  // Random set id that binds all shard images to one hide operation
  const uint8_t *set_id;
  // Expanded shard filename pattern checked before any file is written
  const char *template_text;
  // Payload file descriptor read sequentially by the shard writer
  int payload_file_descriptor;
  // Count of shard files committed so failure cleanup knows what to remove
  uint32_t *shards_written;
  // Plain payload bytes planned for each shard
  const uint32_t *per_shard_plain_len;
  // Ciphertext bytes expected for each shard after encryption
  const uint64_t *per_shard_cipher_len;
  // Packed manifest length already staged at the start of g_cli_workspace.inner
  size_t manifest_len;
  // Master key expanded into per-shard keys inside the writer
  uint8_t *master_key;
  // Stable embed seed derived from the cover with LSBs masked out
  const uint8_t *embed_seed;
  // Header scratch shared with the orchestrator so salt setup stays centralized
  plainsight_split_outer_v2 *outer_header;
} plainsight_cli_hide_shard_write_context;

void plainsight_cli_hide_shard_log_preflight(const plainsight_info_report *report, const char *template_text,
                                             uint64_t payload_file_size);

// Removes only shard paths that were written during the current failed run
void plainsight_cli_hide_shard_cleanup_outputs(const plainsight_hide_options *options,
                                               const char *template_text, uint32_t shards_written);

// Writes each shard image from an already planned and preflighted split operation
plainsight_error
plainsight_cli_hide_shard_write_outputs(const plainsight_cli_hide_shard_write_context *context);

#endif
