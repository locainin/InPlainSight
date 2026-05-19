#ifndef PLAINSIGHT_IMAGE_WEBP_H
#define PLAINSIGHT_IMAGE_WEBP_H

#include "../error.h"
#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

// Reads a WEBP image into RGB bytes
// Decode path supports webp as input-only carrier source
plainsight_error plainsight_image_webp_read(const char *path, plainsight_image *image);

#ifdef __cplusplus
}
#endif

#endif
