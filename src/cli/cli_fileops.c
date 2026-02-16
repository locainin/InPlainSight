#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "cli_internal.h"

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

plainsight_error plainsight_cli_read_exact_bytes(int file_descriptor,
                                 uint8_t *out,
                                 size_t to_read,
                                 size_t *read_out) {
    size_t total_read = 0u;

    if (out == NULL || read_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // The API contract is "read exactly N bytes"
    // This is used for split mode where chunk sizes are planned up front
    while (total_read < to_read) {
        ssize_t read_now = read(file_descriptor, out + total_read, to_read - total_read);
        if (read_now > 0) {
            total_read += (size_t)read_now;
            continue;
        }
        if (read_now == 0) {
            // EOF before the planned length means input bytes changed between plan and read
            return PLAINSIGHT_ERR_BAD_FORMAT;
        }
        if (errno == EINTR || errno == EAGAIN) {
            // Retryable failures keep file read robust under signals and some odd IO sources
            continue;
        }
        return PLAINSIGHT_ERR_IO;
    }

    *read_out = total_read;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_write_all_fd(int file_descriptor, const uint8_t *data, size_t data_len) {
    size_t written_total = 0u;

    if (data == NULL && data_len > 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Loop until all bytes are written or an error occurs
    // write can return short counts even for regular files in some cases
    while (written_total < data_len) {
        ssize_t written_now = write(file_descriptor, data + written_total, data_len - written_total);
        if (written_now > 0) {
            written_total += (size_t)written_now;
            continue;
        }
        if (written_now < 0 && (errno == EINTR || errno == EAGAIN)) {
            // Retryable failures keep writes robust under signals
            continue;
        }
        return PLAINSIGHT_ERR_IO;
    }

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_open_temp_output_exclusive(const char *final_path,
                                           char *temp_path,
                                           size_t temp_cap,
                                           int *file_descriptor_out) {
    int exists_flag = 0;
    size_t path_len = 0u;
    size_t index = 0u;

    if (final_path == NULL || temp_path == NULL || file_descriptor_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Overwrite is refused so automation cannot accidentally destroy output files
    if (plainsight_cli_path_exists(final_path, &exists_flag) != PLAINSIGHT_OK) {
        return PLAINSIGHT_ERR_IO;
    }
    if (exists_flag != 0) {
        return PLAINSIGHT_ERR_IO;
    }

    // Compute final_path length without relying on formatting helpers
    while (final_path[path_len] != '\0') {
        path_len++;
    }

    // temp is "<final>.tmp"
    if (path_len + 4u + 1u > temp_cap) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    for (index = 0u; index < path_len; index++) {
        temp_path[index] = final_path[index];
    }
    temp_path[path_len + 0u] = '.';
    temp_path[path_len + 1u] = 't';
    temp_path[path_len + 2u] = 'm';
    temp_path[path_len + 3u] = 'p';
    temp_path[path_len + 4u] = '\0';

    // 0600 keeps recovered payload private by default
    *file_descriptor_out = open(temp_path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (*file_descriptor_out < 0) {
        return PLAINSIGHT_ERR_IO;
    }

    return PLAINSIGHT_OK;
}

