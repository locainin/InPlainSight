#ifndef PLAINSIGHT_IMAGE_SCRATCH_H
#define PLAINSIGHT_IMAGE_SCRATCH_H

#include <stdint.h>

#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

// Shared bounded file buffers for image backends
// Image operations are single-threaded, so backends can reuse this storage safely
extern uint8_t g_plainsight_image_input_bytes[PLAINSIGHT_MAX_IMAGE_FILE_BYTES];
extern uint8_t g_plainsight_image_output_bytes[PLAINSIGHT_MAX_IMAGE_FILE_BYTES];

#ifdef __cplusplus
}
#endif

#endif
