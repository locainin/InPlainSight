// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>

#include "../../include/image/image.h"
#include "../../include/image/image_scratch.h"
#include "../../include/image/image_bmp.h"
#include "../../include/io.h"
#include "../../include/securemem.h"


static uint16_t plainsight_bmp_read_u16_le(const uint8_t *bytes) {
    uint16_t value = 0u;
    value = (uint16_t)bytes[0];
    value |= (uint16_t)((uint16_t)bytes[1] << 8u);
    return value;
}

static uint32_t plainsight_bmp_read_u32_le(const uint8_t *bytes) {
    uint32_t value = 0u;
    value = (uint32_t)bytes[0];
    value |= (uint32_t)bytes[1] << 8u;
    value |= (uint32_t)bytes[2] << 16u;
    value |= (uint32_t)bytes[3] << 24u;
    return value;
}

static int32_t plainsight_bmp_read_i32_le(const uint8_t *bytes) {
    return (int32_t)plainsight_bmp_read_u32_le(bytes);
}

static void plainsight_bmp_write_u16_le(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8u) & 0xFFu);
}

static void plainsight_bmp_write_u32_le(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8u) & 0xFFu);
    bytes[2] = (uint8_t)((value >> 16u) & 0xFFu);
    bytes[3] = (uint8_t)((value >> 24u) & 0xFFu);
}

static plainsight_error plainsight_bmp_validate_geometry(uint32_t image_width,
                                         uint32_t image_height,
                                         uint64_t rgb_bytes) {
    if (image_width == 0u || image_height == 0u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    if (image_width > PLAINSIGHT_MAX_IMAGE_DIMENSION || image_height > PLAINSIGHT_MAX_IMAGE_DIMENSION) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    if (rgb_bytes > PLAINSIGHT_MAX_IMAGE_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    return PLAINSIGHT_OK;
}

static plainsight_error plainsight_bmp_compute_row_stride(uint32_t image_width, uint32_t *row_stride_out) {
    uint64_t row_bytes = 0u;
    uint64_t padded_row_bytes = 0u;

    if (row_stride_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    row_bytes = (uint64_t)image_width * 3u;
    padded_row_bytes = (row_bytes + 3u) & ~3u;

    if (padded_row_bytes > UINT32_MAX) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    *row_stride_out = (uint32_t)padded_row_bytes;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_image_bmp_read(const char *path, plainsight_image *image) {
    size_t input_length = 0u;
    uint32_t dib_header_size = 0u;
    int32_t encoded_width = 0;
    int32_t encoded_height = 0;
    uint32_t image_width = 0u;
    uint32_t image_height = 0u;
    uint32_t pixel_data_offset = 0u;
    uint32_t row_stride = 0u;
    uint64_t padded_pixel_bytes = 0u;
    uint64_t rgb_bytes = 0u;
    uint16_t bits_per_pixel = 0u;
    uint16_t planes = 0u;
    uint32_t compression = 0u;
    uint32_t row_index = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    if (path == NULL || image == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // BMP decode writes into caller-provided pixel storage
    if (image->pixels == NULL || image->pixels_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    result_code = plainsight_io_read_file(path, g_plainsight_image_input_bytes, sizeof(g_plainsight_image_input_bytes), &input_length);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // BMP headers need at least 54 bytes for the classic file + DIB blocks
    if (input_length < 54u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    if (g_plainsight_image_input_bytes[0] != (uint8_t)'B' || g_plainsight_image_input_bytes[1] != (uint8_t)'M') {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    pixel_data_offset = plainsight_bmp_read_u32_le(g_plainsight_image_input_bytes + 10u);
    dib_header_size = plainsight_bmp_read_u32_le(g_plainsight_image_input_bytes + 14u);
    encoded_width = plainsight_bmp_read_i32_le(g_plainsight_image_input_bytes + 18u);
    encoded_height = plainsight_bmp_read_i32_le(g_plainsight_image_input_bytes + 22u);
    planes = plainsight_bmp_read_u16_le(g_plainsight_image_input_bytes + 26u);
    bits_per_pixel = plainsight_bmp_read_u16_le(g_plainsight_image_input_bytes + 28u);
    compression = plainsight_bmp_read_u32_le(g_plainsight_image_input_bytes + 30u);

    if (dib_header_size < 40u) {
        return PLAINSIGHT_ERR_UNSUPPORTED;
    }

    // This backend only supports simple uncompressed 24-bit BMP
    if (planes != 1u || bits_per_pixel != 24u || compression != 0u) {
        return PLAINSIGHT_ERR_UNSUPPORTED;
    }

    if (encoded_width <= 0 || encoded_height == 0) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    image_width = (uint32_t)encoded_width;
    if (encoded_height < 0) {
        // Negating INT32_MIN is undefined for signed integers
        // Two's-complement unsigned math gives the absolute magnitude safely
        image_height = (~(uint32_t)encoded_height) + 1u;
    } else {
        image_height = (uint32_t)encoded_height;
    }

    rgb_bytes = (uint64_t)image_width * (uint64_t)image_height * 3u;
    result_code = plainsight_bmp_validate_geometry(image_width, image_height, rgb_bytes);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // pixels_cap guards writes into the decoded RGB buffer
    if (rgb_bytes > (uint64_t)image->pixels_cap) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    result_code = plainsight_bmp_compute_row_stride(image_width, &row_stride);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    padded_pixel_bytes = (uint64_t)row_stride * (uint64_t)image_height;
    if (pixel_data_offset > input_length) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    if (padded_pixel_bytes > (uint64_t)(input_length - (size_t)pixel_data_offset)) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    image->width = image_width;
    image->height = image_height;
    image->channels = 3u;
    image->data_len = (size_t)rgb_bytes;

    for (row_index = 0u; row_index < image_height; row_index++) {
        uint32_t source_row_index = 0u;
        size_t source_row_offset = 0u;
        size_t destination_row_offset = 0u;
        uint32_t column_index = 0u;

        // Positive BMP height means rows are stored bottom-up
        if (encoded_height > 0) {
            source_row_index = image_height - 1u - row_index;
        } else {
            source_row_index = row_index;
        }

        source_row_offset = (size_t)pixel_data_offset + (size_t)source_row_index * (size_t)row_stride;
        destination_row_offset = (size_t)row_index * (size_t)image_width * 3u;

        for (column_index = 0u; column_index < image_width; column_index++) {
            size_t source_pixel_offset = source_row_offset + (size_t)column_index * 3u;
            size_t destination_pixel_offset = destination_row_offset + (size_t)column_index * 3u;
            uint8_t blue_component = g_plainsight_image_input_bytes[source_pixel_offset + 0u];
            uint8_t green_component = g_plainsight_image_input_bytes[source_pixel_offset + 1u];
            uint8_t red_component = g_plainsight_image_input_bytes[source_pixel_offset + 2u];

            // BMP stores BGR while project internals use RGB
            image->pixels[destination_pixel_offset + 0u] = red_component;
            image->pixels[destination_pixel_offset + 1u] = green_component;
            image->pixels[destination_pixel_offset + 2u] = blue_component;
        }
    }

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_image_bmp_write(const char *path, const plainsight_image *image) {
    uint32_t row_stride = 0u;
    uint64_t rgb_bytes = 0u;
    uint64_t padded_pixel_bytes = 0u;
    uint64_t total_file_bytes = 0u;
    uint32_t image_width = 0u;
    uint32_t image_height = 0u;
    uint32_t row_index = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    if (path == NULL || image == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Writer requires bound pixel storage so RGB reads stay within the caller-provided buffer
    if (image->pixels == NULL || image->pixels_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    image_width = image->width;
    image_height = image->height;
    rgb_bytes = (uint64_t)image_width * (uint64_t)image_height * 3u;

    if (image->channels != 3u) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if ((uint64_t)image->data_len != rgb_bytes) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // pixels_cap mismatch indicates corrupted caller state or invalid binding
    if (rgb_bytes > (uint64_t)image->pixels_cap) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    result_code = plainsight_bmp_validate_geometry(image_width, image_height, rgb_bytes);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    result_code = plainsight_bmp_compute_row_stride(image_width, &row_stride);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    padded_pixel_bytes = (uint64_t)row_stride * (uint64_t)image_height;
    total_file_bytes = 54u + padded_pixel_bytes;
    if (total_file_bytes > PLAINSIGHT_MAX_IMAGE_FILE_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    // Clear only the used prefix so row padding bytes are deterministic
    plainsight_secure_zero(g_plainsight_image_output_bytes, (size_t)total_file_bytes);

    g_plainsight_image_output_bytes[0] = (uint8_t)'B';
    g_plainsight_image_output_bytes[1] = (uint8_t)'M';
    plainsight_bmp_write_u32_le(g_plainsight_image_output_bytes + 2u, (uint32_t)total_file_bytes);
    plainsight_bmp_write_u32_le(g_plainsight_image_output_bytes + 10u, 54u);
    plainsight_bmp_write_u32_le(g_plainsight_image_output_bytes + 14u, 40u);
    plainsight_bmp_write_u32_le(g_plainsight_image_output_bytes + 18u, image_width);
    plainsight_bmp_write_u32_le(g_plainsight_image_output_bytes + 22u, image_height);
    plainsight_bmp_write_u16_le(g_plainsight_image_output_bytes + 26u, 1u);
    plainsight_bmp_write_u16_le(g_plainsight_image_output_bytes + 28u, 24u);
    plainsight_bmp_write_u32_le(g_plainsight_image_output_bytes + 30u, 0u);
    plainsight_bmp_write_u32_le(g_plainsight_image_output_bytes + 34u, (uint32_t)padded_pixel_bytes);

    for (row_index = 0u; row_index < image_height; row_index++) {
        uint32_t destination_row_index = image_height - 1u - row_index;
        size_t source_row_offset = (size_t)row_index * (size_t)image_width * 3u;
        size_t destination_row_offset = 54u + (size_t)destination_row_index * (size_t)row_stride;
        uint32_t column_index = 0u;

        for (column_index = 0u; column_index < image_width; column_index++) {
            size_t source_pixel_offset = source_row_offset + (size_t)column_index * 3u;
            size_t destination_pixel_offset = destination_row_offset + (size_t)column_index * 3u;
            uint8_t red_component = image->pixels[source_pixel_offset + 0u];
            uint8_t green_component = image->pixels[source_pixel_offset + 1u];
            uint8_t blue_component = image->pixels[source_pixel_offset + 2u];

            // BMP byte order is BGR
            g_plainsight_image_output_bytes[destination_pixel_offset + 0u] = blue_component;
            g_plainsight_image_output_bytes[destination_pixel_offset + 1u] = green_component;
            g_plainsight_image_output_bytes[destination_pixel_offset + 2u] = red_component;
        }
    }

    return plainsight_io_write_file(path, g_plainsight_image_output_bytes, (size_t)total_file_bytes);
}
