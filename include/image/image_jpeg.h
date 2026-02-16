#ifndef PLAINSIGHT_IMAGE_JPEG_H
#define PLAINSIGHT_IMAGE_JPEG_H

#include "../error.h"
#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

// Reads a JPEG image into RGB bytes
// Caller must bind pixel storage via plainsight_image_bind_storage before calling read
// Backend is decode-only to avoid lossy output surprises
plainsight_error plainsight_image_jpeg_read(const char *path, plainsight_image *image);

// JPEG output is intentionally unsupported for hide output safety
plainsight_error plainsight_image_jpeg_write(const char *path, const plainsight_image *image);

#ifdef __cplusplus
}
#endif

#endif
