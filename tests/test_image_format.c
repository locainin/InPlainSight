#include <stdio.h>

#include "../include/image/image.h"

// This file tests the extension-based image format detection helper
// Format detection is intentionally policy-driven and does not inspect file contents

static int check_true(int condition, const char *message_text) {
    // Keep failure output small and stable across environments
    if (!condition) {
        (void)fputs(message_text, stderr);
        (void)fputs("\n", stderr);
        return 0;
    }

    return 1;
}

int main(void) {
    // Basic lowercase extension checks
    if (!check_true(plainsight_image_detect_format_from_path("cover.png") == PLAINSIGHT_IMAGE_FORMAT_PNG,
                    "png extension detection failed")) {
        return 1;
    }

    // Uppercase extension should be accepted
    if (!check_true(plainsight_image_detect_format_from_path("cover.JXL") == PLAINSIGHT_IMAGE_FORMAT_JXL,
                    "jxl extension detection failed")) {
        return 1;
    }

    if (!check_true(plainsight_image_detect_format_from_path("cover.bmp") == PLAINSIGHT_IMAGE_FORMAT_BMP,
                    "bmp extension detection failed")) {
        return 1;
    }

    if (!check_true(plainsight_image_detect_format_from_path("cover.ppm") == PLAINSIGHT_IMAGE_FORMAT_PPM,
                    "ppm extension detection failed")) {
        return 1;
    }

    // Both jpg and jpeg should resolve to the same format enum
    if (!check_true(plainsight_image_detect_format_from_path("cover.jpg") == PLAINSIGHT_IMAGE_FORMAT_JPEG,
                    "jpg extension detection failed")) {
        return 1;
    }

    if (!check_true(plainsight_image_detect_format_from_path("cover.JPEG") == PLAINSIGHT_IMAGE_FORMAT_JPEG,
                    "jpeg extension detection failed")) {
        return 1;
    }

    if (!check_true(plainsight_image_detect_format_from_path("cover.webp") == PLAINSIGHT_IMAGE_FORMAT_WEBP,
                    "webp extension detection failed")) {
        return 1;
    }

    // Unknown extension should map to unknown and not guess
    if (!check_true(plainsight_image_detect_format_from_path("cover.gif") == PLAINSIGHT_IMAGE_FORMAT_UNKNOWN,
                    "unknown extension detection failed")) {
        return 1;
    }

    return 0;
}
