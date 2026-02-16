#include <stddef.h>
#include <stdint.h>
#include <png.h>

#include "../../include/image/image.h"
#include "../../include/image/image_png.h"

static plainsight_error plainsight_png_validate_geometry(uint32_t width, uint32_t height, uint64_t total_bytes) {
    // Geometry check blocks empty decode output
    if (width == 0u || height == 0u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Dimension cap keeps processing under project limits
    if (width > PLAINSIGHT_MAX_IMAGE_DIMENSION || height > PLAINSIGHT_MAX_IMAGE_DIMENSION) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    // Total byte cap matches fixed in-memory pixel buffer
    if (total_bytes > PLAINSIGHT_MAX_IMAGE_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_image_png_read(const char *path, plainsight_image *image) {
    png_image png_image_state = {0};
    png_alloc_size_t png_image_bytes = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    if (path == NULL || image == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Pixel storage is caller-provided so this backend never uses large stack buffers
    if (image->pixels == NULL || image->pixels_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // png_image API requires version field before any decode call
    png_image_state.version = PNG_IMAGE_VERSION;

    if (png_image_begin_read_from_file(&png_image_state, path) == 0) {
        png_image_free(&png_image_state);
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Force a single internal pixel layout for all backends
    png_image_state.format = PNG_FORMAT_RGB;
    png_image_bytes = PNG_IMAGE_SIZE(png_image_state);

    result_code = plainsight_png_validate_geometry(png_image_state.width,
                                           png_image_state.height,
                                           (uint64_t)png_image_bytes);
    if (result_code != PLAINSIGHT_OK) {
        png_image_free(&png_image_state);
        return result_code;
    }

    // Capacity check is explicit so decode never writes past the bound storage
    if ((uint64_t)png_image_bytes > (uint64_t)image->pixels_cap) {
        png_image_free(&png_image_state);
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    if (png_image_finish_read(&png_image_state, NULL, image->pixels, 0, NULL) == 0) {
        png_image_free(&png_image_state);
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Commit normalized RGB metadata for downstream embed logic
    image->width = png_image_state.width;
    image->height = png_image_state.height;
    image->channels = 3u;
    image->data_len = (size_t)png_image_bytes;

    png_image_free(&png_image_state);
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_image_png_write(const char *path, const plainsight_image *image) {
    png_image png_image_state = {0};
    uint64_t expected_bytes = 0u;

    if (path == NULL || image == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Writer requires bound pixel storage and a consistent pixel length
    if (image->pixels == NULL || image->pixels_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Write path accepts only normalized non-empty RGB images
    if (image->channels != 3u || image->width == 0u || image->height == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // PNG backend expects RGB bytes only
    expected_bytes = (uint64_t)image->width * (uint64_t)image->height * 3u;

    if (plainsight_png_validate_geometry(image->width, image->height, expected_bytes) != PLAINSIGHT_OK) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    if (expected_bytes != (uint64_t)image->data_len) {
        // Mismatched byte count indicates corrupted caller state
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    // pixels_cap mismatch indicates corrupted caller state or wrong binding
    if (expected_bytes > (uint64_t)image->pixels_cap) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    png_image_state.version = PNG_IMAGE_VERSION;
    png_image_state.width = image->width;
    png_image_state.height = image->height;
    png_image_state.format = PNG_FORMAT_RGB;

    if (png_image_write_to_file(&png_image_state, path, 0, image->pixels, 0, NULL) == 0) {
        png_image_free(&png_image_state);
        return PLAINSIGHT_ERR_IO;
    }

    png_image_free(&png_image_state);
    return PLAINSIGHT_OK;
}
