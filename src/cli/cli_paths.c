#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cli_internal.h"

static plainsight_error plainsight_cli_copy_text(const char *source_text,
                                 size_t source_len,
                                 char *out,
                                 size_t out_cap,
                                 size_t *out_len) {
    size_t index = 0u;

    if (out == NULL || out_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (source_text == NULL && source_len > 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (out_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (source_len >= out_cap) {
        // The destination must fit a trailing NUL byte
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    // Explicit byte copy keeps bounds handling obvious
    for (index = 0u; index < source_len; index++) {
        out[index] = source_text[index];
    }
    out[source_len] = '\0';
    *out_len = source_len;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_path_exists(const char *path, int *exists_out) {
    struct stat metadata;

    if (path == NULL || exists_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // stat is used instead of fopen so the check does not create files
    // This keeps "existence checks" side effect free
    if (stat(path, &metadata) == 0) {
        *exists_out = 1;
        return PLAINSIGHT_OK;
    }

    if (errno == ENOENT) {
        // ENOENT is the normal "does not exist" case
        *exists_out = 0;
        return PLAINSIGHT_OK;
    }

    // Any other errno is treated as a hard I/O error
    return PLAINSIGHT_ERR_IO;
}

plainsight_error plainsight_cli_path_is_directory(const char *path, int *is_directory_out) {
    struct stat metadata;

    if (path == NULL || is_directory_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Directory validation is used for split output and split extraction input
    // stat follows symlinks, which is acceptable for a local CLI tool
    if (stat(path, &metadata) != 0) {
        return PLAINSIGHT_ERR_IO;
    }

    *is_directory_out = S_ISDIR(metadata.st_mode) ? 1 : 0;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_join_dir_and_name(const char *dir_path,
                                  const char *file_name,
                                  char *out,
                                  size_t out_cap) {
    size_t dir_len = 0u;
    size_t name_len = 0u;
    size_t written = 0u;
    int needs_slash = 0;

    if (dir_path == NULL || file_name == NULL || out == NULL || out_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Manual length scan avoids extra libc helpers in the hot path
    // This function is used repeatedly in split modes
    while (dir_path[dir_len] != '\0') {
        dir_len++;
    }
    while (file_name[name_len] != '\0') {
        name_len++;
    }

    needs_slash = (dir_len > 0u && dir_path[dir_len - 1u] != '/') ? 1 : 0;

    if (dir_len > SIZE_MAX - name_len) {
        // Overflow guard for dir + name length math
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    if (needs_slash != 0 && dir_len == SIZE_MAX) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    if (dir_len + (size_t)needs_slash + name_len + 1u > out_cap) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    if (plainsight_cli_copy_text(dir_path, dir_len, out, out_cap, &written) != PLAINSIGHT_OK) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    if (needs_slash != 0) {
        // Only one slash is inserted, even when dir_path already ends with '/'
        out[written++] = '/';
        out[written] = '\0';
    }

    if (plainsight_cli_copy_text(file_name, name_len, out + written, out_cap - written, &name_len) != PLAINSIGHT_OK) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_store_image_atomic(const char *final_path, const plainsight_image *image) {
    char temp_name[256];
    char temp_path[1024];
    size_t final_index = 0u;
    size_t basename_start = 0u;
    size_t basename_len = 0u;
    size_t dot_index = 0u;
    size_t temp_index = 0u;
    size_t prefix_len = 0u;
    size_t ext_len = 0u;
    size_t dir_prefix_len = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    if (final_path == NULL || image == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Find basename start so temp file can live next to the final path
    // rename works atomically only within the same filesystem directory
    while (final_path[final_index] != '\0') {
        if (final_path[final_index] == '/') {
            basename_start = final_index + 1u;
        }
        final_index++;
    }

    if (basename_start >= final_index) {
        return PLAINSIGHT_ERR_ARGS;
    }

    basename_len = final_index - basename_start;
    if (basename_len == 0u || basename_len + 8u >= sizeof(temp_name)) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    // Locate last dot inside the basename so the temp name preserves the output extension
    // The output extension is how the image writer backend is selected
    // This is critical because temporary files must be encoded using the same backend as final files
    dot_index = basename_len;
    while (dot_index > 0u) {
        if (final_path[basename_start + dot_index - 1u] == '.') {
            dot_index = dot_index - 1u;
            break;
        }
        dot_index--;
    }
    if (dot_index == 0u || dot_index >= basename_len) {
        // Temp naming depends on having an extension
        return PLAINSIGHT_ERR_ARGS;
    }

    prefix_len = dot_index;
    ext_len = basename_len - dot_index;

    // temp_name becomes ".<prefix>.tmp<ext>" so it still ends with the original extension
    // This keeps the write backend identical for temp and final paths
    temp_name[temp_index++] = '.';

    for (final_index = 0u; final_index < prefix_len; final_index++) {
        temp_name[temp_index++] = final_path[basename_start + final_index];
    }

    temp_name[temp_index++] = '.';
    temp_name[temp_index++] = 't';
    temp_name[temp_index++] = 'm';
    temp_name[temp_index++] = 'p';

    for (final_index = 0u; final_index < ext_len; final_index++) {
        temp_name[temp_index++] = final_path[basename_start + dot_index + final_index];
    }

    if (temp_index >= sizeof(temp_name)) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    temp_name[temp_index] = '\0';

    // temp_path becomes "<dir_prefix><temp_name>"
    dir_prefix_len = basename_start;
    if (dir_prefix_len >= sizeof(temp_path)) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    if (plainsight_cli_copy_text(final_path, dir_prefix_len, temp_path, sizeof(temp_path), &dir_prefix_len) != PLAINSIGHT_OK) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    {
        size_t temp_name_len = 0u;
        while (temp_name[temp_name_len] != '\0') {
            temp_name_len++;
        }
        if (plainsight_cli_copy_text(temp_name,
                             temp_name_len,
                             temp_path + dir_prefix_len,
                             sizeof(temp_path) - dir_prefix_len,
                             &temp_name_len) != PLAINSIGHT_OK) {
            return PLAINSIGHT_ERR_TOO_LARGE;
        }
    }

    // Write image to temp path first so failures do not leave partial final files
    // rename is used at the end to make the final path appear in one step
    // This is best-effort atomic behavior and relies on preflight to avoid overwriting existing files
    result_code = plainsight_cli_store_image(temp_path, image);
    if (result_code != PLAINSIGHT_OK) {
        (void)unlink(temp_path);
        return result_code;
    }

    if (rename(temp_path, final_path) != 0) {
        // rename failure can happen on permission issues or cross-device edge cases
        // temp file is removed so retries do not accumulate junk files
        (void)unlink(temp_path);
        return PLAINSIGHT_ERR_IO;
    }

    return PLAINSIGHT_OK;
}
