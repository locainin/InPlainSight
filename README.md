# InPlainSight

InPlainSight is an educational steganography project written in C (CLI) with a Rust GTK4 UI.

![InPlainSight Studio screenshot](assets/VisualDisplay.png)

## Educational Scope

- This project is for education, research, and security learning
- It uses encryption so the payload stays a secret
- It uses pixel-domain steganography to store those encrypted bytes inside an image
- The Goal of this project was to be able to store text/PDFs within an image.

Two layers are involved:

- Cryptography is the lock: it protects confidentiality and makes edits obvious during extraction
- Steganography is the actual hiding within the image: it decides where encrypted bytes live inside pixels so the file still looks like a normal image file

## Limitations (Read This First)

- Lossy re-encoding destroys pixel-domain stego
  This is why output is kept lossless for embedding modes
- Cover images have finite capacity
  Large payloads may require splitting across multiple shard images
- Dependencies may allocate internally
  The project keeps memory bounded in its own code, but image/crypto libraries can still allocate

## How It Works (High Level)

1. Read the payload bytes (a file, or text from the UI)
2. Derive keys from the passphrase (Argon2id)
3. Encrypt and authenticate (XChaCha20-Poly1305)
4. Derive an image-specific embedding seed from the passphrase and the cover pixels (with LSBs masked)
5. Embed the encrypted container bytes into a lossless cover image (pixel-domain embedding)
6. On extract: derive the same seed, pull bytes back out, verify authentication, then write the original bytes

## CLI Usage

### Plan Capacity Before Hiding

`info` reports how much payload fits and whether splitting will be needed

```bash
./inplainsight info --cover cover.png --method lsb --lsb-bits 1 --density 1.0 --json
```

### Hide (Single Output)

```bash
./inplainsight hide \
  --cover cover.png \
  --payload payload.pdf \
  --output stego.png \
  --passphrase-file passphrase.txt \
  --method lsb
```

### Hide (Auto-Split Into Shards)

Use this when a payload does not fit in one cover

```bash
./inplainsight hide \
  --cover cover.png \
  --payload payload.pdf \
  --output stego.png \
  --passphrase-file passphrase.txt \
  --method lsb \
  --split auto \
  --output-dir stego_shards
```

### Extract (Single Image)

```bash
./inplainsight extract \
  --input stego.png \
  --output recovered_payload.bin \
  --passphrase-file passphrase.txt \
  --method auto
```

### Extract (Shard Directory)

```bash
./inplainsight extract \
  --input-dir stego_shards \
  --output recovered_payload.bin \
  --passphrase-file passphrase.txt \
  --method auto
```

## What It Does

- Hides arbitrary bytes inside supported cover images
- Uses authenticated encryption so extraction fails on tampering or wrong credentials
- Restores the original payload bytes on successful extraction
- Embeds data by modifying the least significant bits (LSBs) of pixel channel values (lossless output only)

## Supported Formats

- Cover input: `.png`, `.jxl`, `.bmp`, `.ppm`, `.jpg`, `.jpeg`, `.webp`
- Stego output: `.png`, `.jxl`, `.bmp`, `.ppm` (lossless only)

## GUI (GTK4)

The GUI is a guided workflow that runs the CLI under the hood and captures logs for troubleshooting

```bash
cd ui
cargo run
```

Optional:

- `INPLAINSIGHT_SKIP_CLI_BUILD=1` skips the C build step used by the UI build script

## Build Notes

- C dependencies: `libsodium`, `libpng`, `libjpeg`, `libwebp`, `libjxl` (optional at runtime, supported when installed)
- This repo intentionally enforces strict compiler warnings and sanitizer builds during testing

## Warnings / Ethics / Responsible Use

- This project is meant for learning and analysis, it is in no way meant for real-world use. 
- It is not suggested to embed anything within an image to send to others. 

The goal is to understand how these techniques work and how they are detected

## More Details

- [Technical design and module breakdown](docs/TECHNICAL.md)
