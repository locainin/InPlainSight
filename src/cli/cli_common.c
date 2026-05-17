// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stdio.h>
#include <string.h>

#include "cli_internal.h"

static const char *plainsight_cli_program_basename(const char *program_path) {
    const char *basename = NULL;
    size_t index = 0u;

    if (program_path == NULL || program_path[0] == '\0') {
        return "inplainsight";
    }

    basename = program_path;
    while (program_path[index] != '\0') {
        if (program_path[index] == '/') {
            basename = program_path + index + 1u;
        }
        index++;
    }

    if (basename[0] == '\0') {
        return "inplainsight";
    }
    return basename;
}

void plainsight_cli_print_usage(const char *program_path) {
    const char *program_name = plainsight_cli_program_basename(program_path);

    (void)fputs("usage:\n", stderr);
    (void)fputs("  ", stderr);
    (void)fputs(program_name, stderr);
    (void)fputs(" hide --cover <file> --payload <file> --output <file> --passphrase-file <file> [--method lsb] [--compress none|auto|zstd]\n",
                stderr);
    (void)fputs("  ", stderr);
    (void)fputs(program_name, stderr);
    (void)fputs(
        " hide --cover <file> --payload <file> --split auto --output-dir <dir> --passphrase-file <file> [--output-template shard_%04u.png] [--method lsb]\n",
        stderr);
    (void)fputs("  ", stderr);
    (void)fputs(program_name, stderr);
    (void)fputs(" extract --input <file> --output <file> --passphrase-file <file> [--method lsb]\n", stderr);
    (void)fputs("  ", stderr);
    (void)fputs(program_name, stderr);
    (void)fputs(" extract --input-dir <dir> --output <file-or-dir> --passphrase-file <file> [--method lsb]\n", stderr);
    (void)fputs("  ", stderr);
    (void)fputs(program_name, stderr);
    (void)fputs(
        " info --cover <file> [--payload <file> | --payload-bytes <n>] [--method lsb] [--lsb-bits 1] [--density 1.0] --json\n",
        stderr);
    (void)fputs("supported image extensions: .png .jxl .bmp .ppm .jpg .jpeg .webp\n", stderr);
    (void)fputs("hide output must be lossless: .png .jxl .bmp .ppm\n", stderr);
}

plainsight_embed_method plainsight_cli_parse_method(const char *method_text, plainsight_error *result_code) {
    if (method_text == NULL || result_code == NULL) {
        if (result_code != NULL) {
            *result_code = PLAINSIGHT_ERR_ARGS;
        }
        return PLAINSIGHT_EMBED_INVALID;
    }

    // Keep method names explicit and stable for scriptable CLI behavior
    if (strcmp(method_text, "lsb") == 0) {
        *result_code = PLAINSIGHT_OK;
        return PLAINSIGHT_EMBED_LSB;
    }

    *result_code = PLAINSIGHT_ERR_ARGS;
    return PLAINSIGHT_EMBED_INVALID;
}

plainsight_error plainsight_cli_load_image(const char *path, plainsight_image *image) {
    plainsight_image_format format = PLAINSIGHT_IMAGE_FORMAT_UNKNOWN;

    if (path == NULL || image == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Extension determines backend to avoid ambiguous magic probing
    // Lossy formats are accepted as inputs but not safe as stego outputs
    format = plainsight_image_detect_format_from_path(path);
    if (format == PLAINSIGHT_IMAGE_FORMAT_PNG) {
        return plainsight_image_png_read(path, image);
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_JXL) {
        return plainsight_image_jxl_read(path, image);
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_BMP) {
        return plainsight_image_bmp_read(path, image);
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_PPM) {
        return plainsight_image_ppm_read(path, image);
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_JPEG) {
        return plainsight_image_jpeg_read(path, image);
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_WEBP) {
        return plainsight_image_webp_read(path, image);
    }
    return PLAINSIGHT_ERR_UNSUPPORTED;
}

plainsight_error plainsight_cli_store_image(const char *path, const plainsight_image *image) {
    plainsight_image_format format = PLAINSIGHT_IMAGE_FORMAT_UNKNOWN;

    if (path == NULL || image == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    format = plainsight_image_detect_format_from_path(path);
    if (format == PLAINSIGHT_IMAGE_FORMAT_PNG) {
        return plainsight_image_png_write(path, image);
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_JXL) {
        return plainsight_image_jxl_write(path, image);
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_BMP) {
        return plainsight_image_bmp_write(path, image);
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_PPM) {
        return plainsight_image_ppm_write(path, image);
    }
    // Pixel-domain stego output must stay lossless to preserve embedded bits
    if (format == PLAINSIGHT_IMAGE_FORMAT_JPEG) {
        return PLAINSIGHT_ERR_UNSUPPORTED;
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_WEBP) {
        return PLAINSIGHT_ERR_UNSUPPORTED;
    }
    return PLAINSIGHT_ERR_UNSUPPORTED;
}
