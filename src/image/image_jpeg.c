// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <jpeglib.h>

#include "../../include/image/image.h"
#include "../../include/image/image_jpeg.h"

// Shared row scratch buffer keeps stack usage small in this backend
// This backend is intentionally non-reentrant in current CLI architecture
static uint8_t g_jpeg_row_buffer[PLAINSIGHT_MAX_IMAGE_DIMENSION * 3u];

typedef struct plainsight_jpeg_error_state {
  struct jpeg_error_mgr manager;
  jmp_buf jump_context;
} plainsight_jpeg_error_state;

static void plainsight_jpeg_error_exit(j_common_ptr common_pointer) {
  plainsight_jpeg_error_state *error_state = NULL;

  if (common_pointer == NULL || common_pointer->err == NULL) {
    return;
  }

  error_state = (plainsight_jpeg_error_state *)common_pointer->err;
  longjmp(error_state->jump_context, 1);
}

typedef struct plainsight_jpeg_geometry {
  uint32_t image_width;
  uint32_t image_height;
  uint64_t rgb_bytes;
} plainsight_jpeg_geometry;

static plainsight_error plainsight_jpeg_validate_geometry(const plainsight_jpeg_geometry *geometry) {
  if (geometry == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  if (geometry->image_width == 0u || geometry->image_height == 0u) {
    return PLAINSIGHT_ERR_BAD_FORMAT;
  }

  if (geometry->image_width > PLAINSIGHT_MAX_IMAGE_DIMENSION ||
      geometry->image_height > PLAINSIGHT_MAX_IMAGE_DIMENSION) {
    return PLAINSIGHT_ERR_TOO_LARGE;
  }

  if (geometry->rgb_bytes > PLAINSIGHT_MAX_IMAGE_BYTES) {
    return PLAINSIGHT_ERR_TOO_LARGE;
  }

  return PLAINSIGHT_OK;
}

plainsight_error plainsight_image_jpeg_read(const char *path, plainsight_image *image) {
  FILE *input_stream = NULL;
  struct jpeg_decompress_struct jpeg_state;
  plainsight_jpeg_error_state error_state;
  uint32_t output_width = 0u;
  uint32_t output_height = 0u;
  size_t output_data_len = 0u;
  uint64_t rgb_bytes = 0u;
  plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

  if (path == NULL || image == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // Caller binds pixel storage so this backend never uses large stack buffers for full images
  if (image->pixels == NULL || image->pixels_cap == 0u) {
    return PLAINSIGHT_ERR_ARGS;
  }

  input_stream = fopen(path, "rb");
  if (input_stream == NULL) {
    return PLAINSIGHT_ERR_IO;
  }

  jpeg_state.err = jpeg_std_error(&error_state.manager);
  error_state.manager.error_exit = plainsight_jpeg_error_exit;

  if (setjmp(error_state.jump_context) != 0) {
    jpeg_destroy_decompress(&jpeg_state);
    (void)fclose(input_stream);
    return PLAINSIGHT_ERR_BAD_FORMAT;
  }

  jpeg_create_decompress(&jpeg_state);
  jpeg_stdio_src(&jpeg_state, input_stream);

  if (jpeg_read_header(&jpeg_state, TRUE) != JPEG_HEADER_OK) {
    jpeg_destroy_decompress(&jpeg_state);
    (void)fclose(input_stream);
    return PLAINSIGHT_ERR_BAD_FORMAT;
  }

  jpeg_state.out_color_space = JCS_RGB;
  // Decoder can fail at start for malformed headers or unsupported streams
  if (jpeg_start_decompress(&jpeg_state) == FALSE) {
    jpeg_destroy_decompress(&jpeg_state);
    (void)fclose(input_stream);
    return PLAINSIGHT_ERR_BAD_FORMAT;
  }

  rgb_bytes = (uint64_t)jpeg_state.output_width * (uint64_t)jpeg_state.output_height * 3u;
  {
    plainsight_jpeg_geometry geometry = {jpeg_state.output_width, jpeg_state.output_height, rgb_bytes};
    result_code = plainsight_jpeg_validate_geometry(&geometry);
  }
  if (result_code != PLAINSIGHT_OK) {
    jpeg_finish_decompress(&jpeg_state);
    jpeg_destroy_decompress(&jpeg_state);
    (void)fclose(input_stream);
    return result_code;
  }

  // pixels_cap prevents decoded rows from exceeding the bound destination storage
  if (rgb_bytes > (uint64_t)image->pixels_cap) {
    jpeg_finish_decompress(&jpeg_state);
    jpeg_destroy_decompress(&jpeg_state);
    (void)fclose(input_stream);
    return PLAINSIGHT_ERR_TOO_LARGE;
  }

  if ((uint64_t)jpeg_state.output_width * 3u > sizeof(g_jpeg_row_buffer)) {
    jpeg_finish_decompress(&jpeg_state);
    jpeg_destroy_decompress(&jpeg_state);
    (void)fclose(input_stream);
    return PLAINSIGHT_ERR_TOO_LARGE;
  }

  while (jpeg_state.output_scanline < jpeg_state.output_height) {
    uint8_t *row_pointer = g_jpeg_row_buffer;
    size_t destination_offset = (size_t)jpeg_state.output_scanline * (size_t)jpeg_state.output_width * 3u;

    if (jpeg_read_scanlines(&jpeg_state, &row_pointer, 1u) != 1u) {
      jpeg_finish_decompress(&jpeg_state);
      jpeg_destroy_decompress(&jpeg_state);
      (void)fclose(input_stream);
      return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    for (size_t pixel_index = 0u; pixel_index < (size_t)jpeg_state.output_width * 3u; pixel_index++) {
      image->pixels[destination_offset + pixel_index] = g_jpeg_row_buffer[pixel_index];
    }
  }

  // Commit output metadata before destroy so no post-destroy fields are read
  output_width = jpeg_state.output_width;
  output_height = jpeg_state.output_height;
  output_data_len = (size_t)rgb_bytes;

  (void)jpeg_finish_decompress(&jpeg_state);
  jpeg_destroy_decompress(&jpeg_state);
  (void)fclose(input_stream);

  image->width = output_width;
  image->height = output_height;
  image->channels = 3u;
  image->data_len = output_data_len;

  return PLAINSIGHT_OK;
}
