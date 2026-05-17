// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/io.h"

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

static plainsight_error plainsight_read_exact_limit(int file_descriptor,
                                    uint8_t *output_bytes,
                                    size_t output_capacity,
                                    size_t *output_length) {
    size_t bytes_used = 0u;

    if (output_bytes == NULL || output_length == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    while (1) {
        ssize_t bytes_read = 0;
        size_t bytes_remaining = 0u;

        if (bytes_used >= output_capacity) {
            uint8_t probe_byte = 0u;
            // Probe one extra byte to detect oversize input early
            bytes_read = read(file_descriptor, &probe_byte, 1u);
            if (bytes_read > 0) {
                return PLAINSIGHT_ERR_TOO_LARGE;
            }
            if (bytes_read == 0) {
                *output_length = bytes_used;
                return PLAINSIGHT_OK;
            }
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            return PLAINSIGHT_ERR_IO;
        }

        bytes_remaining = output_capacity - bytes_used;
        bytes_read = read(file_descriptor, output_bytes + bytes_used, bytes_remaining);
        if (bytes_read > 0) {
            bytes_used += (size_t)bytes_read;
            continue;
        }
        if (bytes_read == 0) {
            *output_length = bytes_used;
            return PLAINSIGHT_OK;
        }
        if (errno == EINTR || errno == EAGAIN) {
            continue;
        }
        return PLAINSIGHT_ERR_IO;
    }
}

plainsight_error plainsight_io_read_file(const char *path, uint8_t *out, size_t out_cap, size_t *out_len) {
    int file_descriptor = -1;
    struct stat file_metadata;
    plainsight_error result_code = PLAINSIGHT_ERR_IO;

    if (path == NULL || out == NULL || out_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // O_CLOEXEC prevents descriptor leaks if this process ever launches children
    file_descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (file_descriptor < 0) {
        return PLAINSIGHT_ERR_IO;
    }

    if (fstat(file_descriptor, &file_metadata) != 0) {
        goto cleanup;
    }

    if (!S_ISREG(file_metadata.st_mode)) {
        result_code = PLAINSIGHT_ERR_UNSUPPORTED;
        goto cleanup;
    }

    if (file_metadata.st_size < 0) {
        result_code = PLAINSIGHT_ERR_IO;
        goto cleanup;
    }

    if ((uint64_t)file_metadata.st_size > (uint64_t)out_cap) {
        result_code = PLAINSIGHT_ERR_TOO_LARGE;
        goto cleanup;
    }

    result_code = plainsight_read_exact_limit(file_descriptor, out, out_cap, out_len);

cleanup:
    if (file_descriptor >= 0) {
        (void)close(file_descriptor);
    }
    return result_code;
}

static plainsight_error plainsight_write_all(int file_descriptor, const uint8_t *data, size_t data_len) {
    size_t bytes_written = 0u;

    while (bytes_written < data_len) {
        // Retry short writes and EINTR so output stays complete
        ssize_t bytes_written_now = write(file_descriptor, data + bytes_written, data_len - bytes_written);
        if (bytes_written_now > 0) {
            bytes_written += (size_t)bytes_written_now;
            continue;
        }
        if (bytes_written_now < 0 && (errno == EINTR || errno == EAGAIN)) {
            continue;
        }
        return PLAINSIGHT_ERR_IO;
    }

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_io_write_file(const char *path, const uint8_t *data, size_t data_len) {
    int file_descriptor = -1;
    plainsight_error result_code = PLAINSIGHT_ERR_IO;

    if (path == NULL || (data == NULL && data_len > 0u)) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // 0600 keeps extracted payload private by default.
    // O_EXCL avoids accidental overwrite. O_NOFOLLOW avoids symlink clobbering on platforms that support it.
    file_descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (file_descriptor < 0) {
        return PLAINSIGHT_ERR_IO;
    }

    result_code = plainsight_write_all(file_descriptor, data, data_len);

    if (close(file_descriptor) != 0 && result_code == PLAINSIGHT_OK) {
        result_code = PLAINSIGHT_ERR_IO;
    }

    return result_code;
}

plainsight_error plainsight_io_read_passphrase_file(const char *path, uint8_t *out, size_t out_cap, size_t *out_len) {
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
    size_t passphrase_length = 0u;

    if (out_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    result_code = plainsight_io_read_file(path, out, out_cap, &passphrase_length);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // Trim newline endings so passphrase matches what users expect
    while (passphrase_length > 0u &&
           (out[passphrase_length - 1u] == (uint8_t)'\n' ||
            out[passphrase_length - 1u] == (uint8_t)'\r')) {
        passphrase_length--;
    }

    if (passphrase_length == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    *out_len = passphrase_length;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_io_get_regular_file_size(const char *path, uint64_t *out_size) {
    int file_descriptor = -1;
    struct stat file_metadata;
    plainsight_error result_code = PLAINSIGHT_ERR_IO;

    if (path == NULL || out_size == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Open with CLOEXEC so helper does not leak descriptors into child processes
    file_descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (file_descriptor < 0) {
        return PLAINSIGHT_ERR_IO;
    }

    if (fstat(file_descriptor, &file_metadata) != 0) {
        goto cleanup;
    }

    if (!S_ISREG(file_metadata.st_mode)) {
        // Non-regular paths are intentionally rejected in this helper
        result_code = PLAINSIGHT_ERR_UNSUPPORTED;
        goto cleanup;
    }

    if (file_metadata.st_size < 0) {
        result_code = PLAINSIGHT_ERR_IO;
        goto cleanup;
    }

    *out_size = (uint64_t)file_metadata.st_size;
    result_code = PLAINSIGHT_OK;

cleanup:
    if (file_descriptor >= 0) {
        (void)close(file_descriptor);
    }
    return result_code;
}

plainsight_error plainsight_io_copy_basename(const char *path, char *out, size_t out_cap, size_t *out_len) {
    size_t path_index = 0u;
    size_t basename_start_index = 0u;
    size_t copied_length = 0u;

    if (path == NULL || out == NULL || out_len == NULL || out_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    while (path[path_index] != '\0') {
        if (path[path_index] == '/') {
            // Keep last slash position as basename start
            basename_start_index = path_index + 1u;
        }
        path_index++;
    }

    if (path_index == basename_start_index) {
        return PLAINSIGHT_ERR_ARGS;
    }

    while (path[basename_start_index + copied_length] != '\0') {
        if (copied_length + 1u >= out_cap) {
            return PLAINSIGHT_ERR_TOO_LARGE;
        }
        out[copied_length] = path[basename_start_index + copied_length];
        copied_length++;
    }

    out[copied_length] = '\0';
    *out_len = copied_length;
    return PLAINSIGHT_OK;
}
