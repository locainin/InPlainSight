#ifndef PLAINSIGHT_IO_H
#define PLAINSIGHT_IO_H

#include <stddef.h>
#include <stdint.h>

#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

// Global file and metadata size bounds
#define PLAINSIGHT_MAX_PASSPHRASE_BYTES 256u
#define PLAINSIGHT_MAX_FILENAME_BYTES 128u
#define PLAINSIGHT_MAX_MIME_BYTES 96u
#define PLAINSIGHT_MAX_SINGLE_PAYLOAD_BYTES (8u * 1024u * 1024u)
#define PLAINSIGHT_MAX_SHARD_PLAINTEXT_BYTES (8u * 1024u * 1024u)
#define PLAINSIGHT_MAX_PAYLOAD_BYTES PLAINSIGHT_MAX_SINGLE_PAYLOAD_BYTES

// Reads a file into caller-provided storage with strict size checks
plainsight_error plainsight_io_read_file(const char *path, uint8_t *out, size_t out_cap, size_t *out_len);

// Writes all bytes to a file path with private permissions
plainsight_error plainsight_io_write_file(const char *path, const uint8_t *data, size_t data_len);

// Reads passphrase bytes and trims trailing CR/LF by length only
// Returned bytes are not NUL-terminated and may contain embedded NUL values
plainsight_error plainsight_io_read_passphrase_file(const char *path, uint8_t *out, size_t out_cap, size_t *out_len);

// Reads regular-file size without loading full contents into memory
// Returns PLAINSIGHT_ERR_UNSUPPORTED when path is not a regular file
plainsight_error plainsight_io_get_regular_file_size(const char *path, uint64_t *out_size);

// Copies basename from path into a fixed-size output buffer
plainsight_error plainsight_io_copy_basename(const char *path, char *out, size_t out_cap, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif
