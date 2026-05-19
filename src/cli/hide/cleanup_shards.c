// InPlainSight C module
// Split hide cleanup removes only files this run already committed

#include <stdint.h>
#include <unistd.h>

#include "headers/shards.h"

void plainsight_cli_hide_shard_cleanup_outputs(const plainsight_hide_options *options,
                                               const char *template_text, uint32_t shards_written) {
  char shard_name[256];
  char shard_path[1024];
  uint32_t delete_index = 0u;

  if (options == NULL || options->output_dir == NULL || template_text == NULL) {
    return;
  }

  // Preflight ensures these paths did not exist before this operation
  // Removing only committed indices avoids touching unrelated files
  for (delete_index = 0u; delete_index < shards_written; delete_index++) {
    if (plainsight_cli_split_format_shard_filename(template_text, delete_index, shard_name,
                                                   sizeof(shard_name)) == PLAINSIGHT_OK &&
        plainsight_cli_join_dir_and_name(options->output_dir, shard_name, shard_path, sizeof(shard_path)) ==
            PLAINSIGHT_OK) {
      (void)unlink(shard_path);
    }
  }
}
