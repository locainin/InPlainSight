#include <stddef.h>
#include <stdint.h>

#include <webp/decode.h>

#include "../../include/image/image.h"
#include "../../include/image/image_webp.h"
#include "../../include/io.h"

static uint8_t g_webp_input_bytes[PLAINSIGHT_MAX_IMAGE_FILE_BYTES];

static plainsight_error plainsight_webp_validate_geometry(uint32_t image_width,
                                          uint32_t image_height,
                                          uint64_t rgb_bytes) {
    // Zero-sized images are rejected before decode output is trusted
    if (image_width == 0u || image_height == 0u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Dimension cap keeps decode work inside fixed project bounds
    if (image_width > PLAINSIGHT_MAX_IMAGE_DIMENSION || image_height > PLAINSIGHT_MAX_IMAGE_DIMENSION) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    // RGB byte cap guards fixed destination storage in plainsight_image
    if (rgb_bytes > PLAINSIGHT_MAX_IMAGE_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_image_webp_read(const char *path, plainsight_image *image) {
    int decoded_width = 0;
    int decoded_height = 0;
    size_t input_length = 0u;
    uint64_t rgb_bytes = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
    uint8_t *decode_pointer = NULL;

    if (path == NULL || image == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Read compressed input into a bounded scratch buffer
    result_code = plainsight_io_read_file(path, g_webp_input_bytes, sizeof(g_webp_input_bytes), &input_length);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // Header probe returns decoded geometry without full pixel decode
    if (WebPGetInfo(g_webp_input_bytes, input_length, &decoded_width, &decoded_height) == 0) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    if (decoded_width <= 0 || decoded_height <= 0) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    rgb_bytes = (uint64_t)(uint32_t)decoded_width * (uint64_t)(uint32_t)decoded_height * 3u;
    result_code = plainsight_webp_validate_geometry((uint32_t)decoded_width, (uint32_t)decoded_height, rgb_bytes);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // Decode directly into final RGB buffer so no extra copy is needed
    decode_pointer = WebPDecodeRGBInto(g_webp_input_bytes,
                                       input_length,
                                       image->pixels,
                                       (size_t)rgb_bytes,
                                       decoded_width * 3);
    if (decode_pointer == NULL) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    image->width = (uint32_t)decoded_width;
    image->height = (uint32_t)decoded_height;
    image->channels = 3u;
    image->data_len = (size_t)rgb_bytes;

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_image_webp_write(const char *path, const plainsight_image *image) {
    // Writing WebP is disabled because output can be lossy for pixel-domain stego
    (void)path;
    (void)image;
    return PLAINSIGHT_ERR_UNSUPPORTED;
}
