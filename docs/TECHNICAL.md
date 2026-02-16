# InPlainSight Technical Notes

This document describes how InPlainSight works at a byte and module level

## Scope

- InPlainSight stores encrypted payload bytes inside image pixels
- Cryptography protects confidentiality and integrity
- Embedding is a storage layer and may still be detected by analysis tooling

## Repository Layout

Core C library and CLI:

- `include/` public headers for the C components
- `src/` implementation
- `src/cli/` CLI parsing and workflows for hide/extract/info
- `src/image/` image decode/encode backends
- `src/embed/` pixel-domain embedding backends
- `src/split/` split-shard container helpers

Rust GTK4 UI:

- `ui/` GTK4 application that runs the CLI and parses `info --json` output

Tests:

- `tests/` C unit and integration tests
- `tests/scripts/` regression runner and CLI audit scripts

## Hard Limits

These limits are enforced to keep memory and runtime bounded on hostile inputs

Image limits (decoded):

- Max dimension: `PLAINSIGHT_MAX_IMAGE_DIMENSION` (8192)
- Max decoded bytes: `PLAINSIGHT_MAX_IMAGE_BYTES` (64 MiB)

Image limits (encoded file):

- Max encoded bytes read/written: `PLAINSIGHT_MAX_IMAGE_FILE_BYTES` (64 MiB)

Payload and metadata:

- Max payload bytes per container/shard: `PLAINSIGHT_MAX_PAYLOAD_BYTES` (8 MiB)
- Max passphrase bytes: `PLAINSIGHT_MAX_PASSPHRASE_BYTES` (256)
- Max filename bytes stored in inner metadata: `PLAINSIGHT_MAX_FILENAME_BYTES` (128)
- Max mime bytes stored in inner metadata: `PLAINSIGHT_MAX_MIME_BYTES` (96)

Split mode:

- Max shards: `PLAINSIGHT_MAX_SHARDS` (1024)
- Max total payload bytes (all shards): `PLAINSIGHT_MAX_TOTAL_PAYLOAD_BYTES` (512 MiB)
- Max directory scan candidates: `PLAINSIGHT_MAX_SET_SCAN_IMAGES` (4096)

## Crypto Overview

Crypto is implemented using libsodium (`include/crypto.h`, `src/crypto.c`)

Algorithms:

- Passphrase KDF: Argon2id via `crypto_pwhash` (stored params in outer headers)
- AEAD: XChaCha20-Poly1305 IETF variant
- Subkey expansion: `crypto_kdf_derive_from_key` with fixed 8-byte context labels

Key separation:

- A passphrase-derived master key is not used directly for AEAD
- Encryption keys are derived from a master key using explicit KDF contexts
- Split shards derive independent per-shard encryption keys

Integrity binding (AAD):

- Split outer headers are serialized into a canonical byte layout
- That byte layout is used as AEAD AAD during encrypt/decrypt
- This prevents shard index/count/ciphertext length edits without detection

## Pixel-Domain Embedding

Embedding lives under `include/embed/embed.h` and `src/embed/`

Current backend:

- `lsb` embeds one bit per selected cover byte by modifying the least significant bit (LSB)

Placement:

- Slots are visited in a deterministic keyed permutation derived from a seed
- A two-phase walk prefers textured regions first and then consumes remaining slots
- Texture scoring masks the LSB so scoring remains stable after embedding

Length prefix:

- The first 64 embedded bits store the payload length in bits in little-endian byte order
- Payload bits follow immediately after the length prefix

Channel stride:

- The embed/extract API receives `cover_channel_stride`
- This is the byte distance to a neighboring sample in the same channel
- For tightly packed RGB this is `3`, for RGBA this is `4`

Notes on detection:

- LSB embedding changes pixel value parity for selected bytes
- Bit-plane visualizers and statistical tests can often highlight these edits, especially on low-texture covers
- Lossy re-encoding destroys pixel-domain LSB data, which is why output is restricted to lossless formats

## Capacity Planning

Capacity planning is shared by:

- CLI hide preflight checks
- `info --json` (used by the UI)

Capacity math lives in `include/capacity.h` and `src/capacity.c`

Inputs:

- decoded cover byte length
- number of LSBs used per eligible cover byte (`--lsb-bits`)
- density (`--density`), stored as per-mille to avoid floating math
- metadata lengths (name + mime), which consume capacity inside the encrypted container

Output:

- usable cover bytes after density filtering
- usable carrier bits
- overhead bytes
- max payload bytes that fit the cover before project cap clamping

## Containers

InPlainSight uses a versioned container format that is encrypted and embedded

There are two outer formats:

- Container v1: single-image payloads
- Split outer v2: shard sets with an encrypted manifest in shard 0

### Container v1 (Single Image)

Outer bytes are plaintext and exist to:

- identify the format (magic + version)
- carry KDF parameters and salt
- carry AEAD nonce
- carry ciphertext length so extraction knows how many bytes to pull

The outer header is packed as:

- `magic[8]` = `HIICTR01`
- `version[1]` = `1`
- `reserved[1]` = `0`
- `kdf_alg[2]` little-endian
- `kdf_opslimit[8]` little-endian
- `kdf_memlimit[8]` little-endian
- `salt[16]`
- `nonce[24]`
- `ciphertext_len[8]` little-endian
- `ciphertext[ciphertext_len]`

The outer fixed length is `PLAINSIGHT_CONTAINER_OUTER_FIXED_BYTES` (76 bytes), followed by ciphertext

Inner bytes are plaintext before encryption and include:

- `payload_len[8]` little-endian
- `name_len[2]` little-endian
- `mime_len[2]` little-endian
- `compression[1]` (currently must be 0)
- `reserved[3]` must be 0
- `name[name_len]`
- `mime[mime_len]`
- `payload[payload_len]`

The inner bytes are encrypted and authenticated as a single AEAD message

### Split Outer v2 (Shard Sets)

Split outer headers are plaintext and exist to:

- identify a split shard (`magic + version`)
- carry KDF params and salt for key derivation
- carry nonce and ciphertext length for the shard AEAD
- carry `set_id`, shard index, and shard count for assembly

The split outer AAD serializer is canonical and includes:

- `magic[8]` = `HIISPL02`
- `version[1]` = `2`
- `flags[1]`
- `kdf_alg[2]` little-endian
- `kdf_opslimit[8]` little-endian
- `kdf_memlimit[8]` little-endian
- `salt[16]`
- `nonce[24]`
- `set_id[16]`
- `shard_index[4]` little-endian
- `shard_count[4]` little-endian
- `ciphertext_len[8]` little-endian

That serialized header is used as AEAD AAD during encrypt/decrypt

Shard plaintext:

- Shard 0: `manifest_bytes || payload_chunk_0`
- Shards 1..N-1: `payload_chunk_i`

#### Manifest (Encrypted, Shard 0)

The manifest is inside shard 0 ciphertext and is not visible without a valid passphrase

Fields:

- `manifest_version[1]` must be `PLAINSIGHT_SPLIT_MANIFEST_VERSION` (1)
- `flags[1]` (fail-closed)
- `set_id[16]` (duplicated for integrity checks)
- `total_plaintext_len[8]` (payload total, excluding manifest)
- `compression_mode[1]` (reserved)
- `chunk_plain_len[4]`
- `shard_count[4]`
- `per_shard_plain_len[shard_count]` (u32 entries)
- optional `per_shard_cipher_len[shard_count]` (u64 entries) when enabled by flags

Validation:

- fail-closed on unknown versions or flags
- set_id and shard_count must match the outer headers
- length sums must match `total_plaintext_len`

## Hide and Extract Workflows

### Hide (Single Image)

High level:

1. Decode the cover image into a fixed RGB buffer
2. Build inner bytes (metadata + payload)
3. Derive encryption key and embedding seed
4. AEAD-encrypt inner bytes into ciphertext
5. Pack outer header + ciphertext into container bytes
6. Embed container bytes into cover pixels
7. Encode a lossless output image

### Hide (Split)

High level:

1. Run the planner once to compute shard count and manifest size
2. Generate `set_id` and store it in each shard outer header
3. Derive a master key once, then derive per-shard subkeys
4. For each shard: encrypt with AAD, embed, and write output atomically

### Extract (Single Image)

High level:

1. Decode the input image into pixels
2. Derive the embedding seed and extract the outer header bytes
3. Validate outer header and extract ciphertext length
4. Extract ciphertext bytes and decrypt
5. Parse inner bytes and write payload output

### Extract (Split)

Extraction does not trust filenames or directory order

High level:

1. Enumerate directory entries up to `PLAINSIGHT_MAX_SET_SCAN_IMAGES`
2. For each candidate: decode image, extract minimal outer header, parse, and group by `set_id`
3. Validate one set (unique indices, shard 0 present, count complete)
4. Process shard 0 first, decrypt, parse manifest, validate lengths
5. Process shards 1..N-1 in index order, decrypt, and append bytes to output
6. Write output atomically and fail closed on any mismatch

## Forensics Notes (What Tools Can Observe)

Even with encryption, the following facts can be observed from the file alone:

- The file is a valid image of a given format
- Pixel values have been modified compared to the original cover
- The outer container header bytes are stored in the embedded bitstream and can be detected by targeted extraction with the correct seed

Common analysis results and what they mean:

- "Magic byte" scanners
  Random-looking ciphertext embedded into pixels can coincidentally contain byte patterns that resemble file signatures
  These are typically false positives unless a full parsing and validation step succeeds

- Bit-plane tools
  LSB embedding can create visible patterns on low-texture covers when viewing low bits directly
  This indicates modifications, not plaintext recovery

## CLI Audit Scripts

Entry point:

- `tests/scripts/run_testing.sh`

CLI audit harness:

- `tests/scripts/cli_audit/run.sh`

Coverage includes:

- `info --json` parsing across supported formats
- hide/extract roundtrips across formats
- wrong-credential behavior
- tamper detection (bit flip)
- basic plaintext marker checks over output images
