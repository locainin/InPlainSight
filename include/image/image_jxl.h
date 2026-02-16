#ifndef PLAINSIGHT_IMAGE_JXL_H
#define PLAINSIGHT_IMAGE_JXL_H

#include "../error.h"
#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

// Loads JPEG XL into RGB pixels
// Caller passes plainsight_image with built-in pixels buffer sized for PLAINSIGHT_MAX_IMAGE_BYTES
// Decode path is strict about RGB8 output layout
plainsight_error plainsight_image_jxl_read(const char *path, plainsight_image *image);

// Stores RGB pixels as lossless JPEG XL
// Writer is configured for lossless roundtrip-safe output
plainsight_error plainsight_image_jxl_write(const char *path, const plainsight_image *image);

#ifdef __cplusplus
}
#endif

#endif
