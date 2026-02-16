#include <stddef.h>

#include <sodium.h>

#include "../include/securemem.h"

void plainsight_secure_zero(void *ptr, size_t len) {
    // No-op on empty input keeps call sites simple
    if (ptr == NULL || len == 0u) {
        return;
    }
    // libsodium provides a wipe primitive designed for secrets
    sodium_memzero(ptr, len);
}

plainsight_error plainsight_secure_lock(void *ptr, size_t len) {
    // Lock helpers reject invalid pointers early
    if (ptr == NULL || len == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Locking is best effort and may fail on constrained systems
    if (sodium_mlock(ptr, len) != 0) {
        return PLAINSIGHT_ERR_UNSUPPORTED;
    }

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_secure_unlock(void *ptr, size_t len) {
    // Unlock follows same argument contract as lock
    if (ptr == NULL || len == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Unlock path mirrors lock path for symmetry and cleanup safety
    if (sodium_munlock(ptr, len) != 0) {
        return PLAINSIGHT_ERR_UNSUPPORTED;
    }

    return PLAINSIGHT_OK;
}
