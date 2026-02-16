#ifndef PLAINSIGHT_CLI_SPLIT_HELPERS_H
#define PLAINSIGHT_CLI_SPLIT_HELPERS_H

#include <stddef.h>
#include <stdint.h>

// These includes are relative to the include/ tree so single-file builds
// that include headers via ../../include/... still resolve dependencies
#include "../embed/embed.h"
#include "../error.h"
#include "../split/outer_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

// Reads exactly to_read bytes from a file descriptor or fails
// This is used for split chunk reads where chunk sizes are planned up front
plainsight_error plainsight_cli_read_exact_bytes(int file_descriptor,
                                 uint8_t *out,
                                 size_t to_read,
                                 size_t *read_out);

// Writes all bytes or fails
// This is used for split extraction assembly into one output file descriptor
plainsight_error plainsight_cli_write_all_fd(int file_descriptor, const uint8_t *data, size_t data_len);

// Opens "<final>.tmp" with O_EXCL so extract writes never overwrite existing outputs
// On success, caller must close the returned file descriptor
plainsight_error plainsight_cli_open_temp_output_exclusive(const char *final_path,
                                           char *temp_path,
                                           size_t temp_cap,
                                           int *file_descriptor_out);

// Builds the default split output template for a cover path
// The default extension is lossless and derived from the cover format when possible
plainsight_error plainsight_cli_split_build_default_template(const char *cover_path, char *out, size_t out_cap);

// Formats one shard output filename using a validated template
plainsight_error plainsight_cli_split_format_shard_filename(const char *template_text,
                                            uint32_t shard_index,
                                            char *out,
                                            size_t out_cap);

// Checks that all shard outputs do not already exist
// This helps keep split output best-effort atomic by failing before any writes
plainsight_error plainsight_cli_split_preflight_outputs(const char *output_dir, const char *template_text, uint32_t shard_count);

// Extracts the embedded container bytes from one stego image, then parses split outer v2
// This returns ciphertext as a pointer into container_out, so container_out must remain valid
plainsight_error plainsight_cli_split_extract_one_shard(const char *path,
                                        plainsight_embed_method method,
                                        const uint8_t *passphrase,
                                        size_t passphrase_len,
                                        uint8_t *container_out,
                                        size_t container_cap,
                                        size_t *container_len_out,
                                        plainsight_split_outer_v2 *outer_out,
                                        const uint8_t **ciphertext_out,
                                        size_t *ciphertext_len_out);

#ifdef __cplusplus
}
#endif

#endif
