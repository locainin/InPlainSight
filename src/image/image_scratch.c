// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include "../../include/image/image_scratch.h"

// One input and one output scratch buffer serve all image codecs
// This avoids paying a separate 64 MiB static allocation per backend
uint8_t g_plainsight_image_input_bytes[PLAINSIGHT_MAX_IMAGE_FILE_BYTES];
uint8_t g_plainsight_image_output_bytes[PLAINSIGHT_MAX_IMAGE_FILE_BYTES];
