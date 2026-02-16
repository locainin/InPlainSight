#ifndef PLAINSIGHT_IMAGE_BMP_H
#define PLAINSIGHT_IMAGE_BMP_H

#include "../error.h"
#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

// Reads a 24-bit uncompressed BMP file into RGB bytes
// Bottom-up BMP rows are normalized into top-down internal layout
plainsight_error plainsight_image_bmp_read(const char *path, plainsight_image *image);

// Writes RGB bytes into a 24-bit uncompressed BMP file
// Writer emits deterministic zeroed padding bytes per row
plainsight_error plainsight_image_bmp_write(const char *path, const plainsight_image *image);

#ifdef __cplusplus
}
#endif

#endif
