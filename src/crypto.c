#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <sodium.h>

#include "../include/crypto.h"

// crypto.c wraps libsodium so higher-level modules can stay small and consistent
// All functions treat inputs as untrusted and enforce explicit bounds

static const char PLAINSIGHT_KDF_CTX_ENC[crypto_kdf_CONTEXTBYTES] = {
    'H', 'I', 'I', 'E', 'N', 'C', '0', '1'
};
static const char PLAINSIGHT_KDF_CTX_SHARD_ENC[crypto_kdf_CONTEXTBYTES] = {
    'H', 'I', 'S', 'E', 'N', 'C', '0', '2'
};
static const char PLAINSIGHT_KDF_CTX_STEGO[crypto_kdf_CONTEXTBYTES] = {
    'H', 'I', 'S', 'T', 'E', 'G', '0', '1'
};
static const uint64_t PLAINSIGHT_KDF_MAX_OPSLIMIT = (uint64_t)crypto_pwhash_OPSLIMIT_MODERATE;
static const uint64_t PLAINSIGHT_KDF_MAX_MEMLIMIT = (uint64_t)crypto_pwhash_MEMLIMIT_MODERATE;

static const uint8_t PLAINSIGHT_STEGO_SALT[crypto_pwhash_SALTBYTES] = {
    (uint8_t)'H', (uint8_t)'I', (uint8_t)'I', (uint8_t)'S',
    (uint8_t)'T', (uint8_t)'E', (uint8_t)'G', (uint8_t)'O',
    (uint8_t)'S', (uint8_t)'A', (uint8_t)'L', (uint8_t)'T',
    (uint8_t)'0', (uint8_t)'0', (uint8_t)'0', (uint8_t)'1'
};

static int plainsight_crypto_alg_supported(uint16_t alg) {
    // Only known-good KDF algorithms are accepted
    if ((int)alg == crypto_pwhash_ALG_ARGON2ID13) {
        return 1;
    }
    if ((int)alg == crypto_pwhash_ALG_DEFAULT) {
        return 1;
    }
    return 0;
}

plainsight_error plainsight_crypto_init(void) {
    // sodium_init must run once before most libsodium operations
    if (sodium_init() < 0) {
        return PLAINSIGHT_ERR_CRYPTO;
    }
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_crypto_fill_random(uint8_t *out, size_t out_len) {
    if (out == NULL || out_len == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // randombytes_buf is libsodium's CSPRNG entry point
    randombytes_buf(out, out_len);
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_crypto_derive_key(const uint8_t *passphrase,
                              size_t passphrase_len,
                              const uint8_t salt[crypto_pwhash_SALTBYTES],
                              const plainsight_kdf_params *params,
                              uint8_t key_out[crypto_aead_xchacha20poly1305_ietf_KEYBYTES]) {
    size_t memlimit = 0u;
    uint8_t master_key[crypto_kdf_KEYBYTES];
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    if (passphrase == NULL || salt == NULL || params == NULL || key_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (passphrase_len == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (passphrase_len > ULLONG_MAX) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (!plainsight_crypto_alg_supported(params->alg)) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Bound KDF parameters so attacker-controlled headers cannot demand absurd cost
    // These checks keep extraction responsive even when the carrier bytes are malicious
    if (params->opslimit < (uint64_t)crypto_pwhash_OPSLIMIT_MIN ||
        params->opslimit > (uint64_t)crypto_pwhash_OPSLIMIT_MAX ||
        params->opslimit > PLAINSIGHT_KDF_MAX_OPSLIMIT) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (params->memlimit < (uint64_t)crypto_pwhash_MEMLIMIT_MIN ||
        params->memlimit > (uint64_t)crypto_pwhash_MEMLIMIT_MAX ||
        params->memlimit > PLAINSIGHT_KDF_MAX_MEMLIMIT) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (params->memlimit > (uint64_t)SIZE_MAX) {
        return PLAINSIGHT_ERR_ARGS;
    }

    memlimit = (size_t)params->memlimit;

    // First derive a master key from passphrase using Argon2id
    // The master key is not used directly for encryption to enforce key separation
    if (crypto_pwhash(master_key,
                      crypto_kdf_KEYBYTES,
                      (const char *)passphrase,
                      (unsigned long long)passphrase_len,
                      salt,
                      (unsigned long long)params->opslimit,
                      memlimit,
                      (int)params->alg) != 0) {
        result_code = PLAINSIGHT_ERR_CRYPTO;
        goto cleanup;
    }

    // Then derive an encryption-only subkey with an explicit context label
    // Context labels prevent accidentally using a key for multiple purposes
    if (crypto_kdf_derive_from_key(key_out,
                                   crypto_aead_xchacha20poly1305_ietf_KEYBYTES,
                                   1ULL,
                                   PLAINSIGHT_KDF_CTX_ENC,
                                   master_key) != 0) {
        result_code = PLAINSIGHT_ERR_CRYPTO;
        goto cleanup;
    }

    result_code = PLAINSIGHT_OK;

cleanup:
    // Wipe master_key even on failure so partial secrets do not linger in memory
    sodium_memzero(master_key, sizeof(master_key));
    return result_code;
}

plainsight_error plainsight_crypto_derive_master_key(const uint8_t *passphrase,
                                     size_t passphrase_len,
                                     const uint8_t salt[crypto_pwhash_SALTBYTES],
                                     const plainsight_kdf_params *params,
                                     uint8_t master_key_out[crypto_kdf_KEYBYTES]) {
    size_t memlimit = 0u;

    if (passphrase == NULL || salt == NULL || params == NULL || master_key_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (passphrase_len == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (passphrase_len > ULLONG_MAX) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (!plainsight_crypto_alg_supported(params->alg)) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (params->opslimit < (uint64_t)crypto_pwhash_OPSLIMIT_MIN ||
        params->opslimit > (uint64_t)crypto_pwhash_OPSLIMIT_MAX ||
        params->opslimit > PLAINSIGHT_KDF_MAX_OPSLIMIT) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (params->memlimit < (uint64_t)crypto_pwhash_MEMLIMIT_MIN ||
        params->memlimit > (uint64_t)crypto_pwhash_MEMLIMIT_MAX ||
        params->memlimit > PLAINSIGHT_KDF_MAX_MEMLIMIT) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (params->memlimit > (uint64_t)SIZE_MAX) {
        return PLAINSIGHT_ERR_ARGS;
    }

    memlimit = (size_t)params->memlimit;

    // Master key uses the expensive Argon2id derivation once per operation
    if (crypto_pwhash(master_key_out,
                      crypto_kdf_KEYBYTES,
                      (const char *)passphrase,
                      (unsigned long long)passphrase_len,
                      salt,
                      (unsigned long long)params->opslimit,
                      memlimit,
                      (int)params->alg) != 0) {
        return PLAINSIGHT_ERR_CRYPTO;
    }

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_crypto_derive_shard_encryption_key(const uint8_t master_key[crypto_kdf_KEYBYTES],
                                               uint64_t shard_index,
                                               uint8_t key_out[crypto_aead_xchacha20poly1305_ietf_KEYBYTES]) {
    if (master_key == NULL || key_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Each shard gets a unique subkey so ciphertexts are independent
    if (crypto_kdf_derive_from_key(key_out,
                                   crypto_aead_xchacha20poly1305_ietf_KEYBYTES,
                                   (unsigned long long)shard_index,
                                   PLAINSIGHT_KDF_CTX_SHARD_ENC,
                                   master_key) != 0) {
        return PLAINSIGHT_ERR_CRYPTO;
    }

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_crypto_encrypt(const uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES],
                           const uint8_t nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES],
                           const uint8_t *plaintext,
                           size_t plaintext_len,
                           uint8_t *ciphertext,
                           size_t ciphertext_cap,
                           size_t *ciphertext_len) {
    return plainsight_crypto_encrypt_with_aad(key,
                                      nonce,
                                      plaintext,
                                      plaintext_len,
                                      NULL,
                                      0u,
                                      ciphertext,
                                      ciphertext_cap,
                                      ciphertext_len);
}

plainsight_error plainsight_crypto_encrypt_with_aad(const uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES],
                                    const uint8_t nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES],
                                    const uint8_t *plaintext,
                                    size_t plaintext_len,
                                    const uint8_t *aad,
                                    size_t aad_len,
                                    uint8_t *ciphertext,
                                    size_t ciphertext_cap,
                                    size_t *ciphertext_len) {
    unsigned long long produced = 0ULL;
    const uint8_t *aad_ptr = NULL;
    unsigned long long aad_len_ull = 0ULL;

    if (key == NULL || nonce == NULL || plaintext == NULL || ciphertext == NULL || ciphertext_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (aad == NULL && aad_len > 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // ULLONG_MAX checks avoid implicit narrowing when calling libsodium APIs
    if (plaintext_len > ULLONG_MAX) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (aad_len > ULLONG_MAX) {
        return PLAINSIGHT_ERR_ARGS;
    }

    aad_ptr = aad;
    aad_len_ull = (unsigned long long)aad_len;

    // Ensure destination has room for ciphertext and auth tag
    // This avoids size_t wrap by using subtraction form
    if (ciphertext_cap < crypto_aead_xchacha20poly1305_ietf_ABYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    if (plaintext_len > ciphertext_cap - crypto_aead_xchacha20poly1305_ietf_ABYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    // AEAD covers both privacy and tamper detection
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(ciphertext,
                                                   &produced,
                                                   plaintext,
                                                   (unsigned long long)plaintext_len,
                                                   aad_ptr,
                                                   aad_len_ull,
                                                   NULL,
                                                   nonce,
                                                   key) != 0) {
        return PLAINSIGHT_ERR_CRYPTO;
    }

    if (produced > SIZE_MAX) {
        return PLAINSIGHT_ERR_INTERNAL;
    }

    *ciphertext_len = (size_t)produced;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_crypto_decrypt(const uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES],
                           const uint8_t nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES],
                           const uint8_t *ciphertext,
                           size_t ciphertext_len,
                           uint8_t *plaintext,
                           size_t plaintext_cap,
                           size_t *plaintext_len) {
    return plainsight_crypto_decrypt_with_aad(key,
                                      nonce,
                                      ciphertext,
                                      ciphertext_len,
                                      NULL,
                                      0u,
                                      plaintext,
                                      plaintext_cap,
                                      plaintext_len);
}

plainsight_error plainsight_crypto_decrypt_with_aad(const uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES],
                                    const uint8_t nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES],
                                    const uint8_t *ciphertext,
                                    size_t ciphertext_len,
                                    const uint8_t *aad,
                                    size_t aad_len,
                                    uint8_t *plaintext,
                                    size_t plaintext_cap,
                                    size_t *plaintext_len) {
    unsigned long long produced = 0ULL;
    const uint8_t *aad_ptr = NULL;
    unsigned long long aad_len_ull = 0ULL;

    if (key == NULL || nonce == NULL || ciphertext == NULL || plaintext == NULL || plaintext_len == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (aad == NULL && aad_len > 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // ciphertext must include a tag, otherwise it is not a valid AEAD blob
    if (ciphertext_len < crypto_aead_xchacha20poly1305_ietf_ABYTES) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    if (ciphertext_len > ULLONG_MAX) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (aad_len > ULLONG_MAX) {
        return PLAINSIGHT_ERR_ARGS;
    }

    aad_ptr = aad;
    aad_len_ull = (unsigned long long)aad_len;

    // Reject if caller buffer cannot hold authenticated plaintext
    // This is a safe early return before calling into libsodium
    if (ciphertext_len - crypto_aead_xchacha20poly1305_ietf_ABYTES > plaintext_cap) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    // Any authentication failure maps to a generic auth error upstream
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(plaintext,
                                                   &produced,
                                                   NULL,
                                                   ciphertext,
                                                   (unsigned long long)ciphertext_len,
                                                   aad_ptr,
                                                   aad_len_ull,
                                                   nonce,
                                                   key) != 0) {
        return PLAINSIGHT_ERR_AUTH;
    }

    if (produced > SIZE_MAX) {
        return PLAINSIGHT_ERR_INTERNAL;
    }

    *plaintext_len = (size_t)produced;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_crypto_seed_from_passphrase_and_cover(const uint8_t *passphrase,
                                                  size_t passphrase_len,
                                                  const uint8_t *cover,
                                                  size_t cover_len,
                                                  uint8_t seed_out[32]) {
    uint8_t stego_master[crypto_kdf_KEYBYTES];
    uint8_t stego_subkey[32];
    uint8_t cover_digest[32];
    crypto_generichash_state state;
    size_t cover_byte_index = 0u;
    int hash_rc = 0;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    if (passphrase == NULL || seed_out == NULL || cover == NULL || passphrase_len == 0u || cover_len == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (passphrase_len > ULLONG_MAX) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Separate stego master key from encryption key material
    // This KDF is independent from container KDF so embedding cannot reuse the same key bytes
    if (crypto_pwhash(stego_master,
                      crypto_kdf_KEYBYTES,
                      (const char *)passphrase,
                      (unsigned long long)passphrase_len,
                      PLAINSIGHT_STEGO_SALT,
                      crypto_pwhash_OPSLIMIT_INTERACTIVE,
                      crypto_pwhash_MEMLIMIT_INTERACTIVE,
                      crypto_pwhash_ALG_ARGON2ID13) != 0) {
        return PLAINSIGHT_ERR_CRYPTO;
    }

    // Context-separated stego subkey avoids cross-use of raw passphrase KDF output
    if (crypto_kdf_derive_from_key(stego_subkey,
                                   sizeof(stego_subkey),
                                   1ULL,
                                   PLAINSIGHT_KDF_CTX_STEGO,
                                   stego_master) != 0) {
        result_code = PLAINSIGHT_ERR_CRYPTO;
        goto cleanup;
    }

    if (crypto_generichash_init(&state, stego_subkey, sizeof(stego_subkey), sizeof(cover_digest)) != 0) {
        result_code = PLAINSIGHT_ERR_CRYPTO;
        goto cleanup;
    }

    // Masking LSB keeps seed stable before and after embedding
    // Only the least significant bit is modified by the embedder
    for (cover_byte_index = 0u; cover_byte_index < cover_len; cover_byte_index++) {
        uint8_t cover_byte_without_lsb = (uint8_t)(cover[cover_byte_index] & 0xFEu);
        hash_rc = crypto_generichash_update(&state, &cover_byte_without_lsb, 1u);
        if (hash_rc != 0) {
            result_code = PLAINSIGHT_ERR_CRYPTO;
            goto cleanup;
        }
    }

    if (crypto_generichash_final(&state, cover_digest, sizeof(cover_digest)) != 0) {
        result_code = PLAINSIGHT_ERR_CRYPTO;
        goto cleanup;
    }

    // Final keyed digest becomes the permutation seed used by embed code
    // This additional hash step keeps the output fixed-size and uniformly distributed
    if (crypto_generichash(seed_out,
                           32u,
                           cover_digest,
                           sizeof(cover_digest),
                           stego_subkey,
                           sizeof(stego_subkey)) != 0) {
        result_code = PLAINSIGHT_ERR_CRYPTO;
        goto cleanup;
    }

    result_code = PLAINSIGHT_OK;

cleanup:
    // Wipe all transient secrets before returning
    sodium_memzero(stego_master, sizeof(stego_master));
    sodium_memzero(stego_subkey, sizeof(stego_subkey));
    sodium_memzero(cover_digest, sizeof(cover_digest));
    return result_code;
}
