// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../include/compress.h"
#include "../include/io.h"

// This file tests the bounded zstd helper wrappers

static uint8_t g_plain[4096];
static uint8_t g_compressed[8192];
static uint8_t g_roundtrip[4096];

static int check_true(int condition, const char *message) {
    // Keep failure output direct so sanitizer logs stay readable
    if (!condition) {
        (void)fputs(message, stderr);
        (void)fputs("\n", stderr);
        return 0;
    }
    return 1;
}

int main(void) {
    size_t compressed_len = 0u;
    size_t roundtrip_len = 0u;
    size_t index = 0u;

    // Repeated text should compress while still preserving exact bytes
    for (index = 0u; index < sizeof(g_plain); index++) {
        g_plain[index] = (uint8_t)("plain sight test payload "[index % 25u]);
    }

    if (!check_true(plainsight_compress_zstd(g_plain,
                                             sizeof(g_plain),
                                             g_compressed,
                                             sizeof(g_compressed),
                                             &compressed_len) == PLAINSIGHT_OK,
                    "zstd compression failed")) {
        return 1;
    }
    if (!check_true(compressed_len < sizeof(g_plain), "zstd did not reduce repeated payload")) {
        return 1;
    }

    if (!check_true(plainsight_decompress_zstd(g_compressed,
                                               compressed_len,
                                               g_roundtrip,
                                               sizeof(g_roundtrip),
                                               &roundtrip_len) == PLAINSIGHT_OK,
                    "zstd decompression failed")) {
        return 1;
    }
    if (!check_true(roundtrip_len == sizeof(g_plain), "zstd roundtrip length mismatch")) {
        return 1;
    }
    if (!check_true(memcmp(g_plain, g_roundtrip, sizeof(g_plain)) == 0, "zstd roundtrip bytes mismatch")) {
        return 1;
    }

    // Malformed input must reject instead of producing partial output
    g_compressed[0] ^= 0x7Fu;
    if (!check_true(plainsight_decompress_zstd(g_compressed,
                                               compressed_len,
                                               g_roundtrip,
                                               sizeof(g_roundtrip),
                                               &roundtrip_len) == PLAINSIGHT_ERR_BAD_FORMAT,
                    "malformed zstd input should fail")) {
        return 1;
    }

    return 0;
}
