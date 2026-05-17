// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../include/image/image.h"
#include "../../include/image/image_scratch.h"
#include "../../include/image/image_ppm.h"
#include "../../include/io.h"


static int plainsight_ppm_is_whitespace(uint8_t character) {
    return character == (uint8_t)' ' ||
           character == (uint8_t)'\t' ||
           character == (uint8_t)'\n' ||
           character == (uint8_t)'\r' ||
           character == (uint8_t)'\f' ||
           character == (uint8_t)'\v';
}

static plainsight_error plainsight_ppm_next_token(const uint8_t *input_bytes,
                                  size_t input_length,
                                  size_t *cursor_offset,
                                  char *token_out,
                                  size_t token_capacity,
                                  size_t *token_length_out) {
    size_t token_length = 0u;
    size_t cursor = 0u;

    if (input_bytes == NULL || cursor_offset == NULL || token_out == NULL || token_capacity < 2u || token_length_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    cursor = *cursor_offset;

    while (cursor < input_length) {
        uint8_t current_byte = input_bytes[cursor];

        if (plainsight_ppm_is_whitespace(current_byte)) {
            cursor++;
            continue;
        }

        // PPM comments start with # and continue to end of line
        if (current_byte == (uint8_t)'#') {
            cursor++;
            while (cursor < input_length && input_bytes[cursor] != (uint8_t)'\n') {
                cursor++;
            }
            continue;
        }

        break;
    }

    if (cursor >= input_length) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    while (cursor < input_length) {
        uint8_t current_byte = input_bytes[cursor];
        if (plainsight_ppm_is_whitespace(current_byte) || current_byte == (uint8_t)'#') {
            break;
        }

        if (token_length + 1u >= token_capacity) {
            return PLAINSIGHT_ERR_BAD_FORMAT;
        }

        token_out[token_length] = (char)current_byte;
        token_length++;
        cursor++;
    }

    if (token_length == 0u) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    token_out[token_length] = '\0';
    *token_length_out = token_length;
    *cursor_offset = cursor;
    return PLAINSIGHT_OK;
}

static plainsight_error plainsight_ppm_consume_raster_delimiter(const uint8_t *input_bytes,
                                                size_t input_length,
                                                size_t *cursor_offset) {
    size_t cursor = 0u;

    if (input_bytes == NULL || cursor_offset == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    cursor = *cursor_offset;
    if (cursor >= input_length) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // After maxval there is exactly one delimiter byte before raw pixels
    if (!plainsight_ppm_is_whitespace(input_bytes[cursor])) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    *cursor_offset = cursor + 1u;
    return PLAINSIGHT_OK;
}

static plainsight_error plainsight_ppm_parse_u32_token(const char *token_text, uint32_t *value_out) {
    char *parse_end = NULL;
    unsigned long parsed_value = 0ul;

    if (token_text == NULL || value_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    errno = 0;
    parsed_value = strtoul(token_text, &parse_end, 10);
    if (errno != 0 || parse_end == NULL || *parse_end != '\0') {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    if (parsed_value == 0ul || parsed_value > (unsigned long)UINT32_MAX) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    *value_out = (uint32_t)parsed_value;
    return PLAINSIGHT_OK;
}

static plainsight_error plainsight_ppm_validate_geometry(uint32_t image_width,
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

static plainsight_error plainsight_ppm_copy_bytes(uint8_t *destination,
                                  size_t destination_capacity,
                                  size_t *destination_offset,
                                  const uint8_t *source,
                                  size_t source_length) {
    size_t source_index = 0u;

    if (destination == NULL || destination_offset == NULL || source == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (*destination_offset > destination_capacity) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    if (source_length > (destination_capacity - *destination_offset)) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    for (source_index = 0u; source_index < source_length; source_index++) {
        destination[*destination_offset + source_index] = source[source_index];
    }
    *destination_offset += source_length;
    return PLAINSIGHT_OK;
}

static plainsight_error plainsight_ppm_append_ascii(uint8_t *destination,
                                    size_t destination_capacity,
                                    size_t *destination_offset,
                                    const char *text_value) {
    size_t text_length = 0u;

    if (destination == NULL || destination_offset == NULL || text_value == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    while (text_value[text_length] != '\0') {
        text_length++;
    }

    return plainsight_ppm_copy_bytes(destination,
                             destination_capacity,
                             destination_offset,
                             (const uint8_t *)text_value,
                             text_length);
}

static plainsight_error plainsight_ppm_append_u32_decimal(uint8_t *destination,
                                          size_t destination_capacity,
                                          size_t *destination_offset,
                                          uint32_t numeric_value) {
    uint8_t reverse_digits[10u];
    uint8_t forward_digits[10u];
    size_t reverse_length = 0u;
    size_t digit_index = 0u;
    uint32_t remaining_value = numeric_value;

    if (destination == NULL || destination_offset == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Decimal conversion is explicit to avoid formatter APIs
    if (remaining_value == 0u) {
        reverse_digits[reverse_length] = (uint8_t)'0';
        reverse_length++;
    } else {
        while (remaining_value > 0u && reverse_length < sizeof(reverse_digits)) {
            uint32_t digit_value = remaining_value % 10u;
            reverse_digits[reverse_length] = (uint8_t)('0' + digit_value);
            reverse_length++;
            remaining_value /= 10u;
        }
    }

    if (reverse_length == 0u || reverse_length > sizeof(reverse_digits) || remaining_value != 0u) {
        return PLAINSIGHT_ERR_INTERNAL;
    }

    for (digit_index = 0u; digit_index < reverse_length; digit_index++) {
        forward_digits[digit_index] = reverse_digits[reverse_length - digit_index - 1u];
    }

    return plainsight_ppm_copy_bytes(destination,
                             destination_capacity,
                             destination_offset,
                             forward_digits,
                             reverse_length);
}

plainsight_error plainsight_image_ppm_read(const char *path, plainsight_image *image) {
    size_t input_length = 0u;
    size_t cursor_offset = 0u;
    char token_text[64];
    size_t token_length = 0u;
    uint32_t image_width = 0u;
    uint32_t image_height = 0u;
    uint32_t max_channel_value = 0u;
    uint64_t rgb_bytes = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
    size_t pixel_index = 0u;

    if (path == NULL || image == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // PPM decode writes into caller-provided pixel storage
    if (image->pixels == NULL || image->pixels_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    result_code = plainsight_io_read_file(path, g_plainsight_image_input_bytes, sizeof(g_plainsight_image_input_bytes), &input_length);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    result_code = plainsight_ppm_next_token(g_plainsight_image_input_bytes,
                                    input_length,
                                    &cursor_offset,
                                    token_text,
                                    sizeof(token_text),
                                    &token_length);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }
    if (token_length != 2u || token_text[0] != 'P' || token_text[1] != '6') {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    result_code = plainsight_ppm_next_token(g_plainsight_image_input_bytes,
                                    input_length,
                                    &cursor_offset,
                                    token_text,
                                    sizeof(token_text),
                                    &token_length);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }
    result_code = plainsight_ppm_parse_u32_token(token_text, &image_width);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    result_code = plainsight_ppm_next_token(g_plainsight_image_input_bytes,
                                    input_length,
                                    &cursor_offset,
                                    token_text,
                                    sizeof(token_text),
                                    &token_length);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }
    result_code = plainsight_ppm_parse_u32_token(token_text, &image_height);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    result_code = plainsight_ppm_next_token(g_plainsight_image_input_bytes,
                                    input_length,
                                    &cursor_offset,
                                    token_text,
                                    sizeof(token_text),
                                    &token_length);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }
    result_code = plainsight_ppm_parse_u32_token(token_text, &max_channel_value);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    if (max_channel_value != 255u) {
        return PLAINSIGHT_ERR_UNSUPPORTED;
    }

    rgb_bytes = (uint64_t)image_width * (uint64_t)image_height * 3u;
    result_code = plainsight_ppm_validate_geometry(image_width, image_height, rgb_bytes);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // pixels_cap guards the destination storage before the raster is copied
    if (rgb_bytes > (uint64_t)image->pixels_cap) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    result_code = plainsight_ppm_consume_raster_delimiter(g_plainsight_image_input_bytes, input_length, &cursor_offset);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    if ((size_t)rgb_bytes > (input_length - cursor_offset)) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // Enforce exact length so trailing bytes cannot smuggle extra side data
    if ((size_t)rgb_bytes != (input_length - cursor_offset)) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    image->width = image_width;
    image->height = image_height;
    image->channels = 3u;
    image->data_len = (size_t)rgb_bytes;

    for (pixel_index = 0u; pixel_index < (size_t)rgb_bytes; pixel_index++) {
        image->pixels[pixel_index] = g_plainsight_image_input_bytes[cursor_offset + pixel_index];
    }

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_image_ppm_write(const char *path, const plainsight_image *image) {
    uint64_t rgb_bytes = 0u;
    uint64_t total_output_bytes = 0u;
    uint8_t header_text[64];
    size_t header_length = 0u;
    size_t output_offset = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
    uint32_t image_width = 0u;
    uint32_t image_height = 0u;

    if (path == NULL || image == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Writer requires bound pixel storage and consistent RGB byte length
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

    result_code = plainsight_ppm_validate_geometry(image_width, image_height, rgb_bytes);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    result_code = plainsight_ppm_append_ascii(header_text, sizeof(header_text), &header_length, "P6\n");
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    result_code = plainsight_ppm_append_u32_decimal(header_text, sizeof(header_text), &header_length, image_width);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    result_code = plainsight_ppm_append_ascii(header_text, sizeof(header_text), &header_length, " ");
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    result_code = plainsight_ppm_append_u32_decimal(header_text, sizeof(header_text), &header_length, image_height);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    result_code = plainsight_ppm_append_ascii(header_text, sizeof(header_text), &header_length, "\n255\n");
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    total_output_bytes = (uint64_t)header_length + rgb_bytes;
    if (total_output_bytes > PLAINSIGHT_MAX_IMAGE_FILE_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    result_code = plainsight_ppm_copy_bytes(g_plainsight_image_output_bytes,
                                    sizeof(g_plainsight_image_output_bytes),
                                    &output_offset,
                                    header_text,
                                    header_length);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    result_code = plainsight_ppm_copy_bytes(g_plainsight_image_output_bytes,
                                    sizeof(g_plainsight_image_output_bytes),
                                    &output_offset,
                                    image->pixels,
                                    (size_t)rgb_bytes);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    return plainsight_io_write_file(path, g_plainsight_image_output_bytes, output_offset);
}
