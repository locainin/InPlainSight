#include <stddef.h>

#include "../../include/image/image.h"

static int plainsight_char_lower_equal(char first_character, char second_character) {
    // Inline lowercase conversion keeps extension checks case-insensitive
    if (first_character >= 'A' && first_character <= 'Z') {
        first_character = (char)(first_character - 'A' + 'a');
    }
    if (second_character >= 'A' && second_character <= 'Z') {
        second_character = (char)(second_character - 'A' + 'a');
    }
    return first_character == second_character;
}

static int plainsight_suffix_equals(const char *path, const char *suffix) {
    size_t path_len = 0u;
    size_t suffix_len = 0u;
    size_t suffix_index = 0u;

    if (path == NULL || suffix == NULL) {
        return 0;
    }

    // Manual length scan avoids dependency on extra helpers
    while (path[path_len] != '\0') {
        path_len++;
    }
    while (suffix[suffix_len] != '\0') {
        suffix_len++;
    }

    if (suffix_len > path_len) {
        return 0;
    }

    // Compare only the final suffix bytes using case-insensitive match
    for (suffix_index = 0u; suffix_index < suffix_len; suffix_index++) {
        if (!plainsight_char_lower_equal(path[path_len - suffix_len + suffix_index], suffix[suffix_index])) {
            return 0;
        }
    }

    return 1;
}

plainsight_image_format plainsight_image_detect_format_from_path(const char *path) {
    if (path == NULL) {
        return PLAINSIGHT_IMAGE_FORMAT_UNKNOWN;
    }

    // Extension routing is strict so users control output format explicitly
    if (plainsight_suffix_equals(path, ".png")) {
        return PLAINSIGHT_IMAGE_FORMAT_PNG;
    }

    if (plainsight_suffix_equals(path, ".jxl")) {
        return PLAINSIGHT_IMAGE_FORMAT_JXL;
    }

    if (plainsight_suffix_equals(path, ".bmp") || plainsight_suffix_equals(path, ".dib")) {
        return PLAINSIGHT_IMAGE_FORMAT_BMP;
    }

    if (plainsight_suffix_equals(path, ".ppm")) {
        return PLAINSIGHT_IMAGE_FORMAT_PPM;
    }

    if (plainsight_suffix_equals(path, ".jpg") || plainsight_suffix_equals(path, ".jpeg")) {
        return PLAINSIGHT_IMAGE_FORMAT_JPEG;
    }

    if (plainsight_suffix_equals(path, ".webp")) {
        return PLAINSIGHT_IMAGE_FORMAT_WEBP;
    }

    return PLAINSIGHT_IMAGE_FORMAT_UNKNOWN;
}
