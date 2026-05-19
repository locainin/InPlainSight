# Technical Notes

This file explains the implementation without repeating the source code

## Shape

InPlainSight has one core pipeline:

- Decode image bytes into bounded project buffers
- Pack payload metadata and payload bytes
- Encrypt and authenticate the packed bytes
- Embed the encrypted container into selected image bytes
- Write only lossless stego output

The Rust UI does not implement steganography or crypto. It validates fields, builds CLI arguments, runs the C binary, and shows the CLI output plan

## Commands

The CLI has three main commands:

- `info` decodes the cover and returns capacity planning JSON
- `hide` creates one stego image or a split set
- `extract` recovers one stego image or a split set

Path handling accepts `~/...` for the current user. The expansion is bounded in C by `PLAINSIGHT_MAX_PATH_BYTES` and mirrored in the UI for validation and display

## Preflight

Preflight lives in `src/info.c`

It calculates capacity from:

- decoded cover byte count
- LSB bits per carrier byte
- density
- container overhead
- payload name length
- MIME length
- payload size when one is provided
- split manifest overhead when multiple shards are needed

The UI must not guess split status. It waits for `info --json` and uses the returned plan fields

## LSB Embedding

The only implemented embed method is `lsb`

The public enum leaves room for future methods, but the current parser and UI route to LSB. The technical behavior is in `src/embed/embed_lsb.c`

LSB embedding writes bits into the least significant bit of selected image bytes. The carrier order is deterministic:

- a stego subkey is derived from the passphrase
- the cover bytes are hashed with their LSBs cleared
- that hash becomes a placement seed
- the seed drives a full permutation over candidate carrier bytes
- a texture score changes slot priority without changing total capacity

Clearing the LSB before hashing keeps hide and extract aligned. The same cover should derive the same placement order before and after embedding

## Crypto

Crypto lives in `src/crypto.c` and `include/crypto.h`

The project does not expose a menu of encryption algorithms. The implementation uses libsodium for fixed primitives:

- Argon2id for passphrase-based key derivation
- XChaCha20-Poly1305 for authenticated encryption
- libsodium random bytes for salts, nonces, and split set ids
- libsodium KDF contexts for key separation
- BLAKE2b through `crypto_generichash` for cover-bound placement seeds

Single-image hide derives one encryption subkey from the passphrase and a random salt. Split hide derives one master key, then derives a separate encryption key for each shard

The outer container stores KDF parameters so extraction can replay the same derivation. Extraction only accepts supported libsodium KDF algorithm ids and caps KDF cost before running it, so a modified carrier cannot request unreasonable memory or CPU work

Wrong passphrases, modified ciphertext, modified authenticated metadata, or invalid container bytes all collapse to an authentication-style failure at the CLI boundary

## Containers

Single-image output uses container v1 from `src/container.c`

The outer container is public framing. It carries:

- magic bytes
- version
- KDF algorithm id
- KDF operation limit
- KDF memory limit
- salt
- nonce
- ciphertext length

The inner container is encrypted. It carries:

- original payload length
- payload file name
- MIME bytes
- compression mode
- payload bytes

Inner names are validated before packing and sanitized before extraction writes a recovered filename

## Split Containers

Split output uses split outer v2 from `src/split/outer_v2.c`

Every shard carries public split framing:

- magic bytes and version
- KDF parameters
- salt
- nonce
- set id
- shard index
- shard count
- ciphertext length

The public shard header is serialized as AEAD additional authenticated data. That means extraction can read it before decrypting, but decryption fails if it has been changed

Shard 0 also carries an encrypted manifest. The manifest records the set id, total payload length, shard count, per-shard plaintext lengths, and per-shard ciphertext lengths. Extraction uses it to validate the complete set before writing recovered bytes

Filenames are not trusted for ordering. Shard headers and the manifest decide the set

## Compression

Compression uses zstd through `src/compress.c`

Single-image hide can store:

- no compression
- zstd compression
- automatic zstd trial compression

Automatic compression tries a small set of zstd levels and keeps the smallest result only when it is smaller than the original payload

Split hide currently rejects compression. Split payload bytes are read and encrypted in bounded chunks, which keeps memory use stable for larger payloads

## Memory Bounds

The C code uses fixed project buffers for core data paths

Important limits:

- `PLAINSIGHT_MAX_IMAGE_BYTES`
- `PLAINSIGHT_MAX_SINGLE_PAYLOAD_BYTES`
- `PLAINSIGHT_MAX_SHARD_PLAINTEXT_BYTES`
- `PLAINSIGHT_MAX_TOTAL_PAYLOAD_BYTES`
- `PLAINSIGHT_MAX_SHARDS`
- `PLAINSIGHT_MAX_PATH_BYTES`
- `PLAINSIGHT_MAX_PASSPHRASE_BYTES`

External libraries can allocate internally. Project-owned buffers still enforce local bounds before data is packed, encrypted, embedded, or written

## Output Safety

Output writes are conservative:

- existing output files are refused
- temp files are written next to the final path
- final paths are committed only after successful writes
- extracted payload files use private permissions
- split output paths are preflighted before shard writes begin

This reduces overwrite risk and avoids leaving a partial split set when an output path is already occupied

## UI

The UI lives in `ui/src`

Main areas:

- `app_chrome` builds window chrome, sidebar, and footer
- `app_panels` builds visible workflow panels
- `app_execution` runs CLI operations and maps results back into the UI
- `command_builder` builds CLI argument lists
- `validation` checks user input before a command runs
- `path_utils` expands `~/...` for execution and compacts home paths for display

Step three is driven by CLI preflight. It should show one-image output only when preflight says one image fits. It should show split output only when preflight says multiple images are required

## Verification

Default optimized build:

```sh
make
```

Full project check:

```sh
make check
```

C-only check:

```sh
make verify-c
```

Rust-only check:

```sh
make verify-rust
```

`make check` covers sanitizer builds, C tests, clang-tidy, Rust formatting, Rust clippy, and Rust tests
