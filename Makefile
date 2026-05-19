GCC ?= gcc
CLANG ?= clang
PKG_CONFIG ?= pkg-config

# Project targets and build roots
PROJECT := inplainsight
BUILD_DIR := build

# Application source list grouped by module
APP_SRCS := \
	src/main.c \
	src/cli/core/workspace.c \
	src/cli/common/image_io.c \
	src/cli/parse/values.c \
	src/cli/parse/hide.c \
	src/cli/parse/extract.c \
	src/cli/parse/info.c \
	src/cli/hide/write_single.c \
	src/cli/hide/report_single.c \
	src/cli/hide/compress_single.c \
	src/cli/hide/write_shards.c \
	src/cli/hide/report_shards.c \
	src/cli/hide/cleanup_shards.c \
	src/cli/hide/shard_writer.c \
	src/cli/extract/recover_single.c \
	src/cli/extract/recover_split.c \
	src/cli/info/command.c \
	src/cli/core/dispatch.c \
	src/cli/filesystem/paths.c \
	src/cli/filesystem/fileops.c \
	src/cli/split/naming.c \
	src/cli/split/shard_read.c \
	src/cli/split/shard_set_scan.c \
	src/cli/split/payload_assembly.c \
	src/cli/split/output_resolution.c \
	src/error.c \
	src/capacity.c \
	src/info.c \
	src/io.c \
	src/compress.c \
	src/securemem.c \
	src/container.c \
	src/crypto.c \
	src/embed/embed.c \
	src/embed/embed_lsb.c \
	src/split/outer_v2.c \
	src/split/manifest.c \
	src/split/plan.c \
	src/split/collect.c \
	src/image/image_format.c \
	src/image/image_scratch.c \
	src/image/image_png.c \
	src/image/image_jxl.c \
	src/image/image_jpeg.c \
	src/image/image_bmp.c \
	src/image/image_ppm.c \
	src/image/image_webp.c

CORE_SRCS := $(filter-out src/main.c,$(APP_SRCS))
TEST_SRCS := tests/test_container.c tests/test_compress.c tests/test_crypto_kat.c tests/test_roundtrip.c tests/test_image_format.c tests/test_capacity.c tests/test_io.c tests/test_split_outer_v2.c tests/test_split_manifest.c tests/test_split_collect.c tests/test_split_plan.c tests/test_crypto_aad.c
ALL_HEADERS := $(shell fd -e h . include src)
ALL_C_SRCS := $(APP_SRCS) $(TEST_SRCS)
RUST_UI_DIR := ui

WARN_FLAGS := -Wall -Wextra -Wformat=2 -Wconversion -Wshadow -Wpedantic -Wstrict-prototypes -Wmissing-prototypes -Werror
SAN_FLAGS := -std=c11 -O2 -g -fno-omit-frame-pointer -fsanitize=address,undefined,leak -D_FORTIFY_SOURCE=3 -fstack-protector-strong -fno-sanitize-recover=all $(WARN_FLAGS)
REL_FLAGS := -std=c11 -Wall -Wextra -O3 -march=native -flto -DNDEBUG
REL_LDFLAGS := -flto
SAN_LDFLAGS := -fsanitize=address,undefined,leak
CLANG_REL_FLAGS := -std=c11 -Wall -Wextra -O3 -march=native -DNDEBUG -flto=thin
CLANG_REL_LDFLAGS := -fuse-ld=lld -flto=thin

SODIUM_CFLAGS := $(shell $(PKG_CONFIG) --cflags libsodium 2>/dev/null)
SODIUM_LIBS := $(shell $(PKG_CONFIG) --libs libsodium 2>/dev/null)
PNG_CFLAGS := $(shell $(PKG_CONFIG) --cflags libpng 2>/dev/null)
PNG_LIBS := $(shell $(PKG_CONFIG) --libs libpng 2>/dev/null)
JXL_CFLAGS := $(shell $(PKG_CONFIG) --cflags libjxl 2>/dev/null)
JXL_LIBS := $(shell $(PKG_CONFIG) --libs libjxl 2>/dev/null)
JXL_THREADS_CFLAGS := $(shell $(PKG_CONFIG) --cflags libjxl_threads 2>/dev/null)
JXL_THREADS_LIBS := $(shell $(PKG_CONFIG) --libs libjxl_threads 2>/dev/null)
JPEG_CFLAGS := $(shell $(PKG_CONFIG) --cflags libjpeg 2>/dev/null)
JPEG_LIBS := $(shell $(PKG_CONFIG) --libs libjpeg 2>/dev/null)
WEBP_CFLAGS := $(shell $(PKG_CONFIG) --cflags libwebp 2>/dev/null)
WEBP_LIBS := $(shell $(PKG_CONFIG) --libs libwebp 2>/dev/null)
ZSTD_CFLAGS := $(shell $(PKG_CONFIG) --cflags libzstd 2>/dev/null)
ZSTD_LIBS := $(shell $(PKG_CONFIG) --libs libzstd 2>/dev/null)

ifeq ($(strip $(SODIUM_LIBS)),)
$(error libsodium is required and was not found via pkg-config)
endif

ifeq ($(strip $(PNG_LIBS)),)
$(error libpng is required and was not found via pkg-config)
endif

ifeq ($(strip $(JPEG_LIBS)),)
$(error libjpeg is required and was not found via pkg-config)
endif

ifeq ($(strip $(WEBP_LIBS)),)
$(error libwebp is required and was not found via pkg-config)
endif

ifeq ($(strip $(ZSTD_LIBS)),)
$(error libzstd is required and was not found via pkg-config)
endif

CPPFLAGS_COMMON := -Iinclude $(SODIUM_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(WEBP_CFLAGS) $(ZSTD_CFLAGS)
LDLIBS_COMMON := $(SODIUM_LIBS) $(PNG_LIBS) $(JPEG_LIBS) $(WEBP_LIBS) $(ZSTD_LIBS)

ifneq ($(strip $(JXL_LIBS)),)
# JPEG XL support is enabled when pkg-config finds both encode and thread libs
CPPFLAGS_COMMON += $(JXL_CFLAGS) $(JXL_THREADS_CFLAGS) -DPLAINSIGHT_HAS_LIBJXL=1
LDLIBS_COMMON += $(JXL_LIBS) $(JXL_THREADS_LIBS)
else
# Build still works without JPEG XL by compiling fallback stubs
CPPFLAGS_COMMON += -DPLAINSIGHT_HAS_LIBJXL=0
endif

GCC_SAN_OBJS := $(patsubst %.c,$(BUILD_DIR)/gcc/sanitize/%.o,$(APP_SRCS))
GCC_REL_OBJS := $(patsubst %.c,$(BUILD_DIR)/gcc/release/%.o,$(APP_SRCS))
CLANG_SAN_OBJS := $(patsubst %.c,$(BUILD_DIR)/clang/sanitize/%.o,$(APP_SRCS))
CLANG_REL_OBJS := $(patsubst %.c,$(BUILD_DIR)/clang/release/%.o,$(APP_SRCS))

GCC_SAN_TEST_BINS := \
	$(BUILD_DIR)/gcc/sanitize/tests/test_container \
	$(BUILD_DIR)/gcc/sanitize/tests/test_compress \
	$(BUILD_DIR)/gcc/sanitize/tests/test_crypto_kat \
	$(BUILD_DIR)/gcc/sanitize/tests/test_roundtrip \
	$(BUILD_DIR)/gcc/sanitize/tests/test_image_format \
	$(BUILD_DIR)/gcc/sanitize/tests/test_capacity \
	$(BUILD_DIR)/gcc/sanitize/tests/test_io \
	$(BUILD_DIR)/gcc/sanitize/tests/test_split_outer_v2 \
	$(BUILD_DIR)/gcc/sanitize/tests/test_split_manifest \
	$(BUILD_DIR)/gcc/sanitize/tests/test_split_collect \
	$(BUILD_DIR)/gcc/sanitize/tests/test_split_plan \
	$(BUILD_DIR)/gcc/sanitize/tests/test_crypto_aad

CLANG_SAN_TEST_BINS := \
	$(BUILD_DIR)/clang/sanitize/tests/test_container \
	$(BUILD_DIR)/clang/sanitize/tests/test_compress \
	$(BUILD_DIR)/clang/sanitize/tests/test_crypto_kat \
	$(BUILD_DIR)/clang/sanitize/tests/test_roundtrip \
	$(BUILD_DIR)/clang/sanitize/tests/test_image_format \
	$(BUILD_DIR)/clang/sanitize/tests/test_capacity \
	$(BUILD_DIR)/clang/sanitize/tests/test_io \
	$(BUILD_DIR)/clang/sanitize/tests/test_split_outer_v2 \
	$(BUILD_DIR)/clang/sanitize/tests/test_split_manifest \
	$(BUILD_DIR)/clang/sanitize/tests/test_split_collect \
	$(BUILD_DIR)/clang/sanitize/tests/test_split_plan \
	$(BUILD_DIR)/clang/sanitize/tests/test_crypto_aad

.PHONY: all check clean deep-clean \
	gcc-sanitize gcc-release clang-sanitize clang-release \
	test-gcc test-clang build-tests-gcc build-tests-clang \
	audit-gcc audit-clang verify-gcc verify-clang verify \
	rust-fmt rust-clippy rust-test verify-rust verify-all \
	rust-clean clean-all verify-all-clean \
	compile-commands c-tidy verify-c

all: gcc-release

check: verify-all

gcc-sanitize: $(PROJECT)-gcc-sanitize

gcc-release: $(PROJECT)

clang-sanitize: $(PROJECT)-clang

clang-release: $(PROJECT)-clang-release

$(PROJECT): $(GCC_REL_OBJS)
	@mkdir -p $(dir $@)
	$(GCC) $(REL_LDFLAGS) -o $@ $^ $(LDLIBS_COMMON)

$(PROJECT)-gcc-sanitize: $(GCC_SAN_OBJS)
	@mkdir -p $(dir $@)
	$(GCC) $(SAN_LDFLAGS) -o $@ $^ $(LDLIBS_COMMON)

$(PROJECT)-clang: $(CLANG_SAN_OBJS)
	@mkdir -p $(dir $@)
	$(CLANG) $(SAN_LDFLAGS) -o $@ $^ $(LDLIBS_COMMON)

$(PROJECT)-clang-release: $(CLANG_REL_OBJS)
	@mkdir -p $(dir $@)
	$(CLANG) $(CLANG_REL_LDFLAGS) -o $@ $^ $(LDLIBS_COMMON)

$(BUILD_DIR)/gcc/sanitize/%.o: %.c $(ALL_HEADERS)
	@mkdir -p $(dir $@)
	$(GCC) $(CPPFLAGS_COMMON) $(SAN_FLAGS) -c $< -o $@

$(BUILD_DIR)/gcc/release/%.o: %.c $(ALL_HEADERS)
	@mkdir -p $(dir $@)
	$(GCC) $(CPPFLAGS_COMMON) $(REL_FLAGS) -c $< -o $@

$(BUILD_DIR)/clang/sanitize/%.o: %.c $(ALL_HEADERS)
	@mkdir -p $(dir $@)
	$(CLANG) $(CPPFLAGS_COMMON) $(SAN_FLAGS) -c $< -o $@

$(BUILD_DIR)/clang/release/%.o: %.c $(ALL_HEADERS)
	@mkdir -p $(dir $@)
	$(CLANG) $(CPPFLAGS_COMMON) $(CLANG_REL_FLAGS) -c $< -o $@

$(BUILD_DIR)/gcc/sanitize/tests/test_container: $(BUILD_DIR)/gcc/sanitize/tests/test_container.o $(patsubst %.c,$(BUILD_DIR)/gcc/sanitize/%.o,src/error.c src/container.c)
	@mkdir -p $(dir $@)
	$(GCC) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/gcc/sanitize/tests/test_compress: $(BUILD_DIR)/gcc/sanitize/tests/test_compress.o $(patsubst %.c,$(BUILD_DIR)/gcc/sanitize/%.o,src/error.c src/compress.c)
	@mkdir -p $(dir $@)
	$(GCC) $(SAN_LDFLAGS) -o $@ $^ $(ZSTD_LIBS)

$(BUILD_DIR)/gcc/sanitize/tests/test_crypto_kat: $(BUILD_DIR)/gcc/sanitize/tests/test_crypto_kat.o $(patsubst %.c,$(BUILD_DIR)/gcc/sanitize/%.o,src/error.c src/crypto.c)
	@mkdir -p $(dir $@)
	$(GCC) $(SAN_LDFLAGS) -o $@ $^ $(SODIUM_LIBS)

$(BUILD_DIR)/gcc/sanitize/tests/test_roundtrip: $(BUILD_DIR)/gcc/sanitize/tests/test_roundtrip.o $(patsubst %.c,$(BUILD_DIR)/gcc/sanitize/%.o,src/error.c src/container.c src/crypto.c src/embed/embed.c src/embed/embed_lsb.c)
	@mkdir -p $(dir $@)
	$(GCC) $(SAN_LDFLAGS) -o $@ $^ $(SODIUM_LIBS)

$(BUILD_DIR)/gcc/sanitize/tests/test_image_format: $(BUILD_DIR)/gcc/sanitize/tests/test_image_format.o $(patsubst %.c,$(BUILD_DIR)/gcc/sanitize/%.o,src/image/image_format.c)
	@mkdir -p $(dir $@)
	$(GCC) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/gcc/sanitize/tests/test_capacity: $(BUILD_DIR)/gcc/sanitize/tests/test_capacity.o $(patsubst %.c,$(BUILD_DIR)/gcc/sanitize/%.o,src/error.c src/capacity.c)
	@mkdir -p $(dir $@)
	$(GCC) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/gcc/sanitize/tests/test_io: $(BUILD_DIR)/gcc/sanitize/tests/test_io.o $(patsubst %.c,$(BUILD_DIR)/gcc/sanitize/%.o,src/error.c src/io.c)
	@mkdir -p $(dir $@)
	$(GCC) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/gcc/sanitize/tests/test_split_outer_v2: $(BUILD_DIR)/gcc/sanitize/tests/test_split_outer_v2.o $(patsubst %.c,$(BUILD_DIR)/gcc/sanitize/%.o,src/error.c src/split/outer_v2.c)
	@mkdir -p $(dir $@)
	$(GCC) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/gcc/sanitize/tests/test_split_manifest: $(BUILD_DIR)/gcc/sanitize/tests/test_split_manifest.o $(patsubst %.c,$(BUILD_DIR)/gcc/sanitize/%.o,src/error.c src/split/manifest.c)
	@mkdir -p $(dir $@)
	$(GCC) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/gcc/sanitize/tests/test_split_collect: $(BUILD_DIR)/gcc/sanitize/tests/test_split_collect.o $(patsubst %.c,$(BUILD_DIR)/gcc/sanitize/%.o,src/error.c src/split/collect.c)
	@mkdir -p $(dir $@)
	$(GCC) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/gcc/sanitize/tests/test_split_plan: $(BUILD_DIR)/gcc/sanitize/tests/test_split_plan.o $(patsubst %.c,$(BUILD_DIR)/gcc/sanitize/%.o,src/error.c src/split/plan.c src/split/manifest.c)
	@mkdir -p $(dir $@)
	$(GCC) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/gcc/sanitize/tests/test_crypto_aad: $(BUILD_DIR)/gcc/sanitize/tests/test_crypto_aad.o $(patsubst %.c,$(BUILD_DIR)/gcc/sanitize/%.o,src/error.c src/crypto.c)
	@mkdir -p $(dir $@)
	$(GCC) $(SAN_LDFLAGS) -o $@ $^ $(SODIUM_LIBS)

$(BUILD_DIR)/clang/sanitize/tests/test_container: $(BUILD_DIR)/clang/sanitize/tests/test_container.o $(patsubst %.c,$(BUILD_DIR)/clang/sanitize/%.o,src/error.c src/container.c)
	@mkdir -p $(dir $@)
	$(CLANG) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/clang/sanitize/tests/test_compress: $(BUILD_DIR)/clang/sanitize/tests/test_compress.o $(patsubst %.c,$(BUILD_DIR)/clang/sanitize/%.o,src/error.c src/compress.c)
	@mkdir -p $(dir $@)
	$(CLANG) $(SAN_LDFLAGS) -o $@ $^ $(ZSTD_LIBS)

$(BUILD_DIR)/clang/sanitize/tests/test_crypto_kat: $(BUILD_DIR)/clang/sanitize/tests/test_crypto_kat.o $(patsubst %.c,$(BUILD_DIR)/clang/sanitize/%.o,src/error.c src/crypto.c)
	@mkdir -p $(dir $@)
	$(CLANG) $(SAN_LDFLAGS) -o $@ $^ $(SODIUM_LIBS)

$(BUILD_DIR)/clang/sanitize/tests/test_roundtrip: $(BUILD_DIR)/clang/sanitize/tests/test_roundtrip.o $(patsubst %.c,$(BUILD_DIR)/clang/sanitize/%.o,src/error.c src/container.c src/crypto.c src/embed/embed.c src/embed/embed_lsb.c)
	@mkdir -p $(dir $@)
	$(CLANG) $(SAN_LDFLAGS) -o $@ $^ $(SODIUM_LIBS)

$(BUILD_DIR)/clang/sanitize/tests/test_image_format: $(BUILD_DIR)/clang/sanitize/tests/test_image_format.o $(patsubst %.c,$(BUILD_DIR)/clang/sanitize/%.o,src/image/image_format.c)
	@mkdir -p $(dir $@)
	$(CLANG) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/clang/sanitize/tests/test_capacity: $(BUILD_DIR)/clang/sanitize/tests/test_capacity.o $(patsubst %.c,$(BUILD_DIR)/clang/sanitize/%.o,src/error.c src/capacity.c)
	@mkdir -p $(dir $@)
	$(CLANG) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/clang/sanitize/tests/test_io: $(BUILD_DIR)/clang/sanitize/tests/test_io.o $(patsubst %.c,$(BUILD_DIR)/clang/sanitize/%.o,src/error.c src/io.c)
	@mkdir -p $(dir $@)
	$(CLANG) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/clang/sanitize/tests/test_split_outer_v2: $(BUILD_DIR)/clang/sanitize/tests/test_split_outer_v2.o $(patsubst %.c,$(BUILD_DIR)/clang/sanitize/%.o,src/error.c src/split/outer_v2.c)
	@mkdir -p $(dir $@)
	$(CLANG) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/clang/sanitize/tests/test_split_manifest: $(BUILD_DIR)/clang/sanitize/tests/test_split_manifest.o $(patsubst %.c,$(BUILD_DIR)/clang/sanitize/%.o,src/error.c src/split/manifest.c)
	@mkdir -p $(dir $@)
	$(CLANG) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/clang/sanitize/tests/test_split_collect: $(BUILD_DIR)/clang/sanitize/tests/test_split_collect.o $(patsubst %.c,$(BUILD_DIR)/clang/sanitize/%.o,src/error.c src/split/collect.c)
	@mkdir -p $(dir $@)
	$(CLANG) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/clang/sanitize/tests/test_split_plan: $(BUILD_DIR)/clang/sanitize/tests/test_split_plan.o $(patsubst %.c,$(BUILD_DIR)/clang/sanitize/%.o,src/error.c src/split/plan.c src/split/manifest.c)
	@mkdir -p $(dir $@)
	$(CLANG) $(SAN_LDFLAGS) -o $@ $^

$(BUILD_DIR)/clang/sanitize/tests/test_crypto_aad: $(BUILD_DIR)/clang/sanitize/tests/test_crypto_aad.o $(patsubst %.c,$(BUILD_DIR)/clang/sanitize/%.o,src/error.c src/crypto.c)
	@mkdir -p $(dir $@)
	$(CLANG) $(SAN_LDFLAGS) -o $@ $^ $(SODIUM_LIBS)

test-gcc: $(GCC_SAN_TEST_BINS)
	$(BUILD_DIR)/gcc/sanitize/tests/test_container
	$(BUILD_DIR)/gcc/sanitize/tests/test_compress
	$(BUILD_DIR)/gcc/sanitize/tests/test_crypto_kat
	$(BUILD_DIR)/gcc/sanitize/tests/test_roundtrip
	$(BUILD_DIR)/gcc/sanitize/tests/test_image_format
	$(BUILD_DIR)/gcc/sanitize/tests/test_capacity
	$(BUILD_DIR)/gcc/sanitize/tests/test_io
	$(BUILD_DIR)/gcc/sanitize/tests/test_split_outer_v2
	$(BUILD_DIR)/gcc/sanitize/tests/test_split_manifest
	$(BUILD_DIR)/gcc/sanitize/tests/test_split_collect
	$(BUILD_DIR)/gcc/sanitize/tests/test_split_plan
	$(BUILD_DIR)/gcc/sanitize/tests/test_crypto_aad

build-tests-gcc: $(GCC_SAN_TEST_BINS)

test-clang: $(CLANG_SAN_TEST_BINS)
	$(BUILD_DIR)/clang/sanitize/tests/test_container
	$(BUILD_DIR)/clang/sanitize/tests/test_compress
	$(BUILD_DIR)/clang/sanitize/tests/test_crypto_kat
	$(BUILD_DIR)/clang/sanitize/tests/test_roundtrip
	$(BUILD_DIR)/clang/sanitize/tests/test_image_format
	$(BUILD_DIR)/clang/sanitize/tests/test_capacity
	$(BUILD_DIR)/clang/sanitize/tests/test_io
	$(BUILD_DIR)/clang/sanitize/tests/test_split_outer_v2
	$(BUILD_DIR)/clang/sanitize/tests/test_split_manifest
	$(BUILD_DIR)/clang/sanitize/tests/test_split_collect
	$(BUILD_DIR)/clang/sanitize/tests/test_split_plan
	$(BUILD_DIR)/clang/sanitize/tests/test_crypto_aad

build-tests-clang: $(CLANG_SAN_TEST_BINS)

audit-gcc:
	@mkdir -p $(BUILD_DIR)/audit/gcc
	@set -e; \
	# Compile each translation unit alone to catch per-file warnings \
	for c in $(ALL_C_SRCS); do \
		obj="$(BUILD_DIR)/audit/gcc/$${c##*/}.o"; \
		$(GCC) $(CPPFLAGS_COMMON) $(SAN_FLAGS) -c "$$c" -o "$$obj"; \
	done

audit-clang:
	@mkdir -p $(BUILD_DIR)/audit/clang
	@set -e; \
	# Same audit loop using clang toolchain \
	for c in $(ALL_C_SRCS); do \
		obj="$(BUILD_DIR)/audit/clang/$${c##*/}.o"; \
		$(CLANG) $(CPPFLAGS_COMMON) $(SAN_FLAGS) -c "$$c" -o "$$obj"; \
	done

verify-gcc: audit-gcc gcc-sanitize test-gcc

verify-clang: audit-clang clang-sanitize test-clang

verify: verify-gcc verify-clang

rust-fmt:
	cd $(RUST_UI_DIR) && cargo fmt --all -- --check

rust-clippy:
	cd $(RUST_UI_DIR) && cargo clippy --all-targets --all-features -- -D warnings

rust-test:
	cd $(RUST_UI_DIR) && cargo test --all-targets

verify-rust: rust-fmt rust-clippy rust-test

verify-c: verify compile-commands c-tidy

verify-all: verify-c verify-rust

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(PROJECT) $(PROJECT)-gcc-sanitize $(PROJECT)-clang $(PROJECT)-clang-release
	rm -f compile_commands.json
	rm -rf final
	rm -f src/*.o src/*/*.o src/*/*/*.o tests/*.o tests/test_container tests/test_compress tests/test_crypto_kat tests/test_roundtrip tests/test_image_format tests/test_capacity tests/test_io tests/test_split_outer_v2 tests/test_split_manifest tests/test_split_collect tests/test_split_plan tests/test_crypto_aad

deep-clean: clean
	rm -f *.gcda *.gcno

rust-clean:
	cd $(RUST_UI_DIR) && cargo clean

clean-all: clean rust-clean

verify-all-clean: clean-all verify-all

compile-commands:
	# Build only, no execution
	# Some intercept-build modes interfere with running ASan binaries
	intercept-build --cdb compile_commands.json $(MAKE) -B gcc-sanitize build-tests-gcc >/dev/null

c-tidy: compile-commands
	# run-clang-tidy uses the compilation database so include paths and feature macros match the real build
	# Only macro-parentheses is excluded because libpng system macros trip it outside project code
	run-clang-tidy -p . -warnings-as-errors='*' \
		-checks='clang-analyzer-*,bugprone-*,-bugprone-macro-parentheses'
