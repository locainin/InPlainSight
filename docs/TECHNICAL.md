# InPlainSight Technical Notes

This document explains how InPlainSight works, from bytes on disk to the main modules.

## Scope

- InPlainSight stores encrypted payload bytes inside image pixels
- Cryptography keeps the payload secret and makes edits show up during extraction
- Embedding is a way to store bytes and can still be spotted by analysis tools

## Mental Model (Plain Language)

InPlainSight solves two different problems:

- Keep the payload secret and detect changes
- Put the encrypted bytes inside an image

Those are handled by two separate layers:

- The crypto layer turns plaintext into encrypted bytes and adds a change check
  If the passphrase is wrong or the bytes were changed, decryption fails
- The stego layer picks which pixel bytes carry the encrypted bits
  This can avoid simple file-signature scanning, but it does not promise stealth

Placement depends on the passphrase and the image.
The seed is derived from the passphrase and the cover pixels, with each pixel byte masked to clear the LSB.
Clearing the LSB keeps the seed stable even after embedding changes those bits.

## Terminology (Simple)

- Passphrase: the secret text used to unlock the payload
- Key: fixed-size secret bytes derived from the passphrase
- Plaintext: the original bytes before encryption
- Ciphertext: the encrypted bytes (what gets embedded)
- Encrypt: make bytes unreadable without the key
- Authenticate: detect whether bytes were changed
- AEAD: encrypt + authenticate in one operation
- KDF: a slow passphrase-to-key function
- AAD: extra bytes that are not encrypted, but are still protected by the change check
- Little-endian: multi-byte numbers stored low byte first

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

These limits keep memory and runtime under control on hostile inputs.

Image limits (decoded):

- Max dimension: `PLAINSIGHT_MAX_IMAGE_DIMENSION` (8192)
- Max decoded bytes: `PLAINSIGHT_MAX_IMAGE_BYTES` (64 MiB)

Image limits (encoded file):

- Max encoded bytes read/written: `PLAINSIGHT_MAX_IMAGE_FILE_BYTES` (64 MiB)

Memory model notes:

- `plainsight_image` does not allocate; callers provide a pixel buffer (`include/image/image.h`)
- The CLI uses one shared RGB buffer in `g_cli_workspace` so pixel storage stays off the stack (`src/cli/cli_workspace.c`)

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

Crypto uses libsodium (`include/crypto.h`, `src/crypto.c`).

Notes:

- The passphrase is run through a slow KDF so guessing costs time and RAM
- The payload is encrypted so it cannot be read without the key
- The payload is authenticated so edits are detected during extract

Algorithms:

- Passphrase KDF: Argon2id via `crypto_pwhash` (params stored in the outer header)
- AEAD: XChaCha20-Poly1305 (IETF variant)
- Subkeys: `crypto_kdf_derive_from_key` with fixed 8-byte context labels

Key separation:

- The passphrase-derived master key is not used directly for AEAD
- Encryption keys are derived using explicit contexts so keys are not reused by accident
- Split shards derive a different key per shard

Integrity binding (AAD):

- In split mode, the public shard header is serialized in one fixed layout
- Those header bytes are passed as AEAD AAD during encrypt/decrypt
- This prevents edits to shard index/count/length fields without detection

## Pixel-Domain Embedding

Embedding lives under `include/embed/embed.h` and `src/embed/`.

Current backend:

- `lsb` embeds one bit per selected cover byte by changing the least significant bit (LSB)

Notes:

- Pixel channels are stored as bytes (0 to 255)
- The LSB is the last bit of the byte
- Flipping the LSB changes a channel value by 1, which is often hard to notice in a normal photo

Placement:

- Slots are visited in a repeatable shuffle based on the seed
- It tries higher-texture areas first, then uses the remaining slots
- Texture scoring clears the LSB so the score does not change after embedding

Length prefix:

- The first 64 embedded bits store the embedded payload length in bytes (little-endian)
- Payload bits follow immediately after the length prefix

Channel stride:

- The embed/extract API receives `cover_channel_stride`
- This is the byte distance between two values from the same channel
- In this project, decoded covers are normalized to RGB, so the stride is `3`

Notes on detection:

- LSB embedding changes the even/odd value of selected bytes
- Bit-plane viewers and stats tests can often highlight these edits, especially on low-texture covers
- Lossy re-encoding destroys pixel-domain LSB data, so output is restricted to lossless formats

## Capacity Planning

Capacity planning is shared by:

- CLI hide preflight checks
- `info --json` (used by the UI)

Capacity math lives in `include/capacity.h` and `src/capacity.c`.

Inputs:

- decoded cover byte length
- number of LSBs used per eligible cover byte (`--lsb-bits`)
- density (`--density`), stored as per-mille (0..1000) to avoid floats
- metadata lengths (name + mime), which consume capacity inside the encrypted container

Notes:

- `info` can model different `--density` values
- `--lsb-bits` exists for future use and is currently fixed to `1`
- The current hide/extract path embeds 1 bit per cover byte and uses full density

Output:

- usable cover bytes after density filtering
- usable carrier bits
- overhead bytes
- max payload bytes that fit the cover before project cap clamping

## Containers

InPlainSight uses a versioned container format that is encrypted and embedded.

There are two outer formats:

- Container v1: single-image payloads
- Split outer v2: shard sets with an encrypted manifest in shard 0

### Container v1 (Single Image)

Outer bytes are plaintext and exist to:

- identify the format (magic + version)
- carry KDF parameters and salt
- carry AEAD nonce
- carry ciphertext length so extraction knows how many bytes to pull

Notes:

- The outer header is not encrypted, but it is embedded into pixels like everything else
- The inner bytes and payload bytes are encrypted

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

The outer fixed length is `PLAINSIGHT_CONTAINER_OUTER_FIXED_BYTES` (76 bytes), followed by ciphertext.

Inner bytes are plaintext before encryption and include:

- `payload_len[8]` little-endian
- `name_len[2]` little-endian
- `mime_len[2]` little-endian
- `compression[1]` (currently must be 0)
- `reserved[3]` must be 0
- `name[name_len]`
- `mime[mime_len]`
- `payload[payload_len]`

The inner bytes are encrypted and authenticated as a single AEAD message.

### Split Outer v2 (Shard Sets)

Split outer headers are plaintext and exist to:

- identify a split shard (`magic + version`)
- carry KDF params and salt for key derivation
- carry nonce and ciphertext length for the shard AEAD
- carry `set_id`, shard index, and shard count for assembly

The split outer AAD serializer includes:

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

Those serialized bytes are used as AEAD AAD during encrypt/decrypt.

Shard plaintext:

- Shard 0: `manifest_bytes || payload_chunk_0`
- Shards 1..N-1: `payload_chunk_i`

#### Manifest (Encrypted, Shard 0)

The manifest is inside shard 0 ciphertext and is not visible without a valid passphrase.

Fields:

- `magic[4]` = `HISM`
- `manifest_version[1]` must be `PLAINSIGHT_SPLIT_MANIFEST_VERSION` (1)
- `flags[1]` (fail-closed on unknown bits)
- `reserved[2]` must be 0
- `set_id[16]` (duplicated for integrity checks)
- `total_plaintext_len[8]` (payload total, excluding manifest)
- `compression_mode[1]` (reserved)
- `reserved[3]` must be 0
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

1. Decode the cover image into a bounded RGB buffer
2. Build inner bytes (metadata + payload)
3. Derive encryption key and embedding seed
4. AEAD-encrypt inner bytes into ciphertext
5. Pack outer header + ciphertext into container bytes
6. Embed container bytes into cover pixels
7. Encode a lossless output image and write it crash-safe (write temp file, then rename, refuses overwrite)

### Hide (Split)

High level:

1. Run the planner once to compute shard count and manifest size
2. Generate `set_id` and store it in each shard outer header
3. Derive a master key once, then derive per-shard subkeys
4. For each shard: encrypt with AAD, embed, and write output crash-safe (write temp file, then rename, refuses overwrite)

### Extract (Single Image)

High level:

1. Decode the input image into pixels
2. Derive the embedding seed and extract the outer header bytes
3. Validate outer header and extract ciphertext length
4. Extract ciphertext bytes and decrypt
5. Parse inner bytes and write payload output crash-safe (exclusive temp file, then rename, refuses overwrite)

### Extract (Split)

Extraction does not trust filenames or directory order.

High level:

1. Enumerate directory entries up to `PLAINSIGHT_MAX_SET_SCAN_IMAGES`
2. For each candidate: decode image, extract minimal outer header, parse, and group by `set_id`
3. Validate one set (unique indices, shard 0 present, count complete)
4. Process shard 0 first, decrypt, parse manifest, validate lengths
5. Process shards 1..N-1 in index order, decrypt, and append bytes to output
6. Write output crash-safe and fail closed on any mismatch

## Forensics Notes (What Tools Can Observe)

Even with encryption, the following facts can be observed from the file alone:

- The file is a valid image of a given format
- Pixel values have been modified compared to the original cover
- The outer container header bytes are stored in the embedded bitstream
- The header is not readable without the correct embedding seed, derived from the passphrase and the cover pixels (with LSBs cleared)

Common analysis results and what they mean:

- "Magic byte" scanners
  Random-looking encrypted bytes can sometimes contain patterns that look like file signatures
  These are usually false positives unless full parsing and validation succeeds

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
