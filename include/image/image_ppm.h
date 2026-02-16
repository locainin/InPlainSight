#ifndef PLAINSIGHT_IMAGE_PPM_H
#define PLAINSIGHT_IMAGE_PPM_H

#include "../error.h"
#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

// Reads a binary PPM (P6) image into RGB bytes
// Decoder enforces strict header and size bounds
plainsight_error plainsight_image_ppm_read(const char *path, plainsight_image *image);

// Writes RGB bytes as a binary PPM (P6) image
// Output is plain lossless RGB with deterministic formatting
plainsight_error plainsight_image_ppm_write(const char *path, const plainsight_image *image);

#ifdef __cplusplus
}
#endif

#endif
