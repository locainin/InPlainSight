#ifndef PLAINSIGHT_CLI_INTERNAL_H
#define PLAINSIGHT_CLI_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include <sodium.h>

#include "../../include/container.h"
#include "../../include/crypto.h"
#include "../../include/embed/embed.h"
#include "../../include/error.h"
#include "../../include/info.h"
#include "../../include/image/image.h"
#include "../../include/image/image_bmp.h"
#include "../../include/image/image_jxl.h"
#include "../../include/image/image_jpeg.h"
#include "../../include/image/image_png.h"
#include "../../include/image/image_ppm.h"
#include "../../include/image/image_webp.h"
#include "../../include/io.h"
#include "../../include/securemem.h"
#include "../../include/cli/cli_split_helpers.h"

typedef struct plainsight_hide_options {
    // Cover path used as carrier image
    const char *cover_path;
    // Payload path read as raw bytes
    const char *payload_path;
    // Output path for stego image
    const char *output_path;
    // Output directory for split shard images
    const char *output_dir;
    // Optional split naming template, for example shard_%04u.png
    const char *output_template;
    // Passphrase source file path
    const char *passphrase_path;
    // Selected embed backend
    plainsight_embed_method method;
    // Enables split planning and shard output mode
    int split_auto;
} plainsight_hide_options;

typedef struct plainsight_extract_options {
    // Input stego image path
    const char *input_path;
    // Input directory for split shard extraction
    const char *input_dir;
    // Output payload file path
    const char *output_path;
    // Passphrase source file path
    const char *passphrase_path;
    // Selected extract backend
    plainsight_embed_method method;
} plainsight_extract_options;

typedef struct plainsight_info_options {
    // Cover image used for capacity and planning calculations
    const char *cover_path;
    // Optional payload path used for fit/split planning
    const char *payload_path;
    // Optional payload length for non-file sources like typed text
    int payload_bytes_provided;
    uint64_t payload_bytes_value;
    // Selected embed method for capacity math
    plainsight_embed_method method;
    // Requested bits-per-byte planning knob
    uint8_t lsb_bits;
    // Requested density in per-mille form
    uint16_t density_per_mille;
    // When set, command emits stable JSON schema to stdout
    int json_output;
} plainsight_info_options;

typedef struct plainsight_workspace {
    // Pixel storage lives outside plainsight_image to avoid accidental stack allocation blowups
    uint8_t image_pixels[PLAINSIGHT_MAX_IMAGE_BYTES];
    // Decoded image view over image_pixels
    plainsight_image image;
    // Plain payload bytes loaded from disk
    uint8_t payload[PLAINSIGHT_MAX_PAYLOAD_BYTES];
    // Inner authenticated plaintext container
    uint8_t inner[PLAINSIGHT_MAX_INNER_BYTES];
    // Encrypted bytes before outer packing
    uint8_t ciphertext[PLAINSIGHT_MAX_CIPHERTEXT_BYTES];
    // Packed outer container used for embedding
    uint8_t container[PLAINSIGHT_MAX_CONTAINER_BYTES];
    // Passphrase bytes loaded from file or memfd
    uint8_t passphrase[PLAINSIGHT_MAX_PASSPHRASE_BYTES];
    // Basename copied from payload path
    char payload_name[PLAINSIGHT_MAX_FILENAME_BYTES];
} plainsight_workspace;

extern plainsight_workspace g_cli_workspace;

// Initializes g_cli_workspace fields that require runtime binding
// This is called once at CLI startup before any image decode or encode occurs
void plainsight_cli_workspace_init(void);

void plainsight_cli_print_usage(const char *program_path);

plainsight_embed_method plainsight_cli_parse_method(const char *method_text, plainsight_error *result_code);

plainsight_error plainsight_cli_load_image(const char *path, plainsight_image *image);
plainsight_error plainsight_cli_store_image(const char *path, const plainsight_image *image);
plainsight_error plainsight_cli_store_image_atomic(const char *path, const plainsight_image *image);

plainsight_error plainsight_cli_path_exists(const char *path, int *exists_out);
plainsight_error plainsight_cli_path_is_directory(const char *path, int *is_directory_out);
plainsight_error plainsight_cli_join_dir_and_name(const char *dir_path,
                                  const char *file_name,
                                  char *out,
                                  size_t out_cap);

plainsight_error plainsight_cli_parse_hide_args(int argc, char **argv, plainsight_hide_options *options);
plainsight_error plainsight_cli_parse_extract_args(int argc, char **argv, plainsight_extract_options *options);
plainsight_error plainsight_cli_parse_info_args(int argc, char **argv, plainsight_info_options *options);

plainsight_error plainsight_cli_run_hide(const plainsight_hide_options *options);
plainsight_error plainsight_cli_run_extract(const plainsight_extract_options *options);
plainsight_error plainsight_cli_run_info(const plainsight_info_options *options);
plainsight_error plainsight_cli_run_hide_split(const plainsight_hide_options *options);
plainsight_error plainsight_cli_run_extract_split(const plainsight_extract_options *options);

#endif
