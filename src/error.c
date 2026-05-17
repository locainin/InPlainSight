// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include "../include/error.h"

const char *plainsight_error_str(plainsight_error code) {
    // Stable strings make scripts and logs easy to reason about
    switch (code) {
        case PLAINSIGHT_OK:
            // Success label keeps return-path checks readable in callers
            return "ok";
        case PLAINSIGHT_ERR_ARGS:
            // Input validation failures map to one generic user-facing string
            return "invalid arguments";
        case PLAINSIGHT_ERR_IO:
            // IO errors avoid leaking low-level errno details through this API
            return "I/O failure";
        case PLAINSIGHT_ERR_TOO_LARGE:
            // Size cap violations are grouped under one deterministic label
            return "input exceeds configured limits";
        case PLAINSIGHT_ERR_BAD_FORMAT:
            // Parse and decode failures report invalid format
            return "invalid format";
        case PLAINSIGHT_ERR_UNSUPPORTED:
            // Unsupported mode or backend path
            return "unsupported operation";
        case PLAINSIGHT_ERR_CRYPTO:
            // Crypto backend failures stay generic for safety
            return "cryptographic failure";
        case PLAINSIGHT_ERR_AUTH:
            // Auth text is normalized to avoid payload existence oracles
            return "no payload found or invalid credentials";
        case PLAINSIGHT_ERR_CAPACITY:
            // Cover size too small for requested container
            return "cover capacity is insufficient";
        case PLAINSIGHT_ERR_INTERNAL:
            // Unexpected control-flow or invariant break
            return "internal failure";
        default:
            // Unknown enum values still map to a stable fallback label
            return "unknown failure";
    }
}
