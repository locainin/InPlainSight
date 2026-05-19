#ifndef PLAINSIGHT_CRYPTO_H
#define PLAINSIGHT_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#include <sodium.h>

#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

// KDF parameters stored in the outer container header
typedef struct plainsight_kdf_params {
    uint64_t opslimit;
    uint64_t memlimit;
    uint16_t alg;
} plainsight_kdf_params;

// Initializes the crypto backend once at process startup
plainsight_error plainsight_crypto_init(void);

// Fills bytes using the system CSPRNG from libsodium
plainsight_error plainsight_crypto_fill_random(uint8_t *out, size_t out_len);

// Derives an encryption key from passphrase + salt + KDF params
plainsight_error plainsight_crypto_derive_key(const uint8_t *passphrase,
                              size_t passphrase_len,
                              const uint8_t salt[crypto_pwhash_SALTBYTES],
                              const plainsight_kdf_params *params,
                              uint8_t key_out[crypto_aead_xchacha20poly1305_ietf_KEYBYTES]);

// Derives a master key from passphrase + salt + KDF params for later subkey expansion
// The master key must be wiped by the caller when no longer needed
plainsight_error plainsight_crypto_derive_master_key(const uint8_t *passphrase,
                                     size_t passphrase_len,
                                     const uint8_t salt[crypto_pwhash_SALTBYTES],
                                     const plainsight_kdf_params *params,
                                     uint8_t master_key_out[crypto_kdf_KEYBYTES]);

// Derives a per-shard encryption key from a master key using an explicit context label
plainsight_error plainsight_crypto_derive_shard_encryption_key(const uint8_t master_key[crypto_kdf_KEYBYTES],
                                               uint64_t shard_index,
                                               uint8_t key_out[crypto_aead_xchacha20poly1305_ietf_KEYBYTES]);

// Authenticated encryption for inner container bytes
plainsight_error plainsight_crypto_encrypt(const uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES],
                           const uint8_t nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES],
                           const uint8_t *plaintext,
                           size_t plaintext_len,
                           uint8_t *ciphertext,
                           size_t ciphertext_cap,
                           size_t *ciphertext_len);

// Authenticated encryption with explicit AAD for integrity-bound public metadata
plainsight_error plainsight_crypto_encrypt_with_aad(const uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES],
                                    const uint8_t nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES],
                                    const uint8_t *plaintext,
                                    size_t plaintext_len,
                                    const uint8_t *aad,
                                    size_t aad_len,
                                    uint8_t *ciphertext,
                                    size_t ciphertext_cap,
                                    size_t *ciphertext_len);

// Authenticated decryption for inner container bytes
plainsight_error plainsight_crypto_decrypt(const uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES],
                           const uint8_t nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES],
                           const uint8_t *ciphertext,
                           size_t ciphertext_len,
                           uint8_t *plaintext,
                           size_t plaintext_cap,
                           size_t *plaintext_len);

// Authenticated decryption with explicit AAD for integrity-bound public metadata
plainsight_error plainsight_crypto_decrypt_with_aad(const uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES],
                                    const uint8_t nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES],
                                    const uint8_t *ciphertext,
                                    size_t ciphertext_len,
                                    const uint8_t *aad,
                                    size_t aad_len,
                                    uint8_t *plaintext,
                                    size_t plaintext_cap,
                                    size_t *plaintext_len);

// Bytes in the stego subkey used to seed cover-dependent embedding order
#define PLAINSIGHT_STEGO_SUBKEY_BYTES 32u

// Derives a stego-only subkey from a passphrase
//
// Parameters:
// - passphrase/passphrase_len: caller-owned passphrase bytes; length must be non-zero
// - subkey_out: 32-byte caller-owned output buffer
//
// Bounds and ownership:
// - no heap allocation is performed
// - subkey_out must be wiped by the caller when no longer needed
//
// Returns PLAINSIGHT_OK on success, or a crypto/argument error
// Complexity: one Argon2id derivation
plainsight_error plainsight_crypto_derive_stego_subkey(const uint8_t *passphrase,
                                       size_t passphrase_len,
                                       uint8_t subkey_out[PLAINSIGHT_STEGO_SUBKEY_BYTES]);

// Derives a deterministic embedding seed from a stego subkey + LSB-masked cover bytes
//
// Parameters:
// - stego_subkey: 32-byte key from plainsight_crypto_derive_stego_subkey
// - cover/cover_len: caller-owned image bytes; length must be non-zero
// - seed_out: 32-byte caller-owned output buffer
//
// Bounds and ownership:
// - no heap allocation is performed
// - cover bytes are read only
//
// Returns PLAINSIGHT_OK on success, or a crypto/argument error
// Complexity: O(cover_len)
plainsight_error plainsight_crypto_seed_from_subkey_and_cover(const uint8_t stego_subkey[PLAINSIGHT_STEGO_SUBKEY_BYTES],
                                              const uint8_t *cover,
                                              size_t cover_len,
                                              uint8_t seed_out[32]);

// Derives a deterministic embedding seed from passphrase + LSB-masked cover bytes
//
// This is a compatibility wrapper for single-image operations
// Split extraction should derive the stego subkey once and reuse it per shard
plainsight_error plainsight_crypto_seed_from_passphrase_and_cover(const uint8_t *passphrase,
                                                  size_t passphrase_len,
                                                  const uint8_t *cover,
                                                  size_t cover_len,
                                                  uint8_t seed_out[32]);

#ifdef __cplusplus
}
#endif

#endif
