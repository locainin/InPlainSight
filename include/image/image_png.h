#ifndef PLAINSIGHT_IMAGE_PNG_H
#define PLAINSIGHT_IMAGE_PNG_H

#include "../error.h"
#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

// Loads PNG into RGB pixels
// Decode path normalizes all input variants to 3-channel RGB
plainsight_error plainsight_image_png_read(const char *path, plainsight_image *image);

// Stores RGB pixels as PNG
// Write path preserves exact pixel bytes for lossless stego output
plainsight_error plainsight_image_png_write(const char *path, const plainsight_image *image);

#ifdef __cplusplus
}
#endif

#endif
