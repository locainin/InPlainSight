#ifndef PLAINSIGHT_SECUREMEM_H
#define PLAINSIGHT_SECUREMEM_H

#include <stddef.h>
#include <stdint.h>

#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

// Zeroes sensitive bytes in a way compilers will not optimize away
// Safe to call regardless of libsodium initialization state
void plainsight_secure_zero(void *ptr, size_t len);

// Best-effort memory lock for secret buffers
// Call plainsight_crypto_init at startup before using lock/unlock helpers
plainsight_error plainsight_secure_lock(void *ptr, size_t len);

// Unlocks a previously locked secret buffer
plainsight_error plainsight_secure_unlock(void *ptr, size_t len);

#ifdef __cplusplus
}
#endif

#endif
