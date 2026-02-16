#ifndef PLAINSIGHT_IMAGE_H
#define PLAINSIGHT_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#include "../error.h"

#ifdef __cplusplus
extern "C" {
#endif

// Safety caps for decoded images and encoded files
#define PLAINSIGHT_MAX_IMAGE_DIMENSION 8192u
#define PLAINSIGHT_MAX_IMAGE_BYTES (64u * 1024u * 1024u)
#define PLAINSIGHT_MAX_IMAGE_FILE_BYTES (64u * 1024u * 1024u)

typedef enum plainsight_image_format {
    PLAINSIGHT_IMAGE_FORMAT_UNKNOWN = 0,
    PLAINSIGHT_IMAGE_FORMAT_PNG,
    PLAINSIGHT_IMAGE_FORMAT_JXL,
    PLAINSIGHT_IMAGE_FORMAT_BMP,
    PLAINSIGHT_IMAGE_FORMAT_PPM,
    PLAINSIGHT_IMAGE_FORMAT_JPEG,
    PLAINSIGHT_IMAGE_FORMAT_WEBP
} plainsight_image_format;

// Unified in-memory image layout used by all backends
// pixels storage is fixed-size and must hold up to PLAINSIGHT_MAX_IMAGE_BYTES
typedef struct plainsight_image {
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    size_t data_len;
    uint8_t pixels[PLAINSIGHT_MAX_IMAGE_BYTES];
} plainsight_image;

// Picks backend based on file extension
plainsight_image_format plainsight_image_detect_format_from_path(const char *path);

#ifdef __cplusplus
}
#endif

#endif
