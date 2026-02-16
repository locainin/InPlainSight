#ifndef PLAINSIGHT_ERROR_H
#define PLAINSIGHT_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

// Shared error codes used across modules
// Values are stable so CLI and UI logs stay predictable
typedef enum plainsight_error {
    PLAINSIGHT_OK = 0,
    PLAINSIGHT_ERR_ARGS,
    PLAINSIGHT_ERR_IO,
    PLAINSIGHT_ERR_TOO_LARGE,
    PLAINSIGHT_ERR_BAD_FORMAT,
    PLAINSIGHT_ERR_UNSUPPORTED,
    PLAINSIGHT_ERR_CRYPTO,
    PLAINSIGHT_ERR_AUTH,
    PLAINSIGHT_ERR_CAPACITY,
    PLAINSIGHT_ERR_INTERNAL
} plainsight_error;

// Converts an error code to a stable readable label
// Returned strings are static and do not require caller cleanup
const char *plainsight_error_str(plainsight_error code);

#ifdef __cplusplus
}
#endif

#endif
