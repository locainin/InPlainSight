// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>

#include <string.h>

#include "../internal.h"

static const char g_template_default_prefix[] = "shard_";

static const char *plainsight_cli_lossless_extension_for_cover(const char *cover_path) {
  plainsight_image_format format = plainsight_image_detect_format_from_path(cover_path);

  if (format == PLAINSIGHT_IMAGE_FORMAT_PNG) {
    // PNG is a lossless pixel format so embedded bits survive re-encoding
    return "png";
  }
  if (format == PLAINSIGHT_IMAGE_FORMAT_JXL) {
    // JXL is supported in lossless mode in this project
    return "jxl";
  }
  if (format == PLAINSIGHT_IMAGE_FORMAT_BMP) {
    // BMP is treated as a raw pixel dump
    return "bmp";
  }
  if (format == PLAINSIGHT_IMAGE_FORMAT_PPM) {
    // PPM is treated as a raw pixel dump
    return "ppm";
  }

  // Lossy inputs decode, but output must stay lossless so LSB bits survive encoding
  // Defaulting to PNG prevents silent lossy output behavior
  return "png";
}

static int plainsight_cli_extension_is_lossless_output(const char *extension_text) {
  if (extension_text == NULL) {
    return 0;
  }
  if (strcmp(extension_text, "png") == 0) {
    return 1;
  }
  if (strcmp(extension_text, "jxl") == 0) {
    return 1;
  }
  if (strcmp(extension_text, "bmp") == 0) {
    return 1;
  }
  if (strcmp(extension_text, "ppm") == 0) {
    return 1;
  }
  return 0;
}

static plainsight_error plainsight_cli_parse_template(const char *template_text, size_t *prefix_len_out,
                                                      unsigned int *pad_width_out,
                                                      const char **extension_out) {
  size_t index = 0u;
  size_t dot_index = 0u;
  size_t percent_index = 0u;
  int saw_percent = 0;
  int saw_dot = 0;
  unsigned int pad_width = 0u;
  int zero_pad = 0;

  if (template_text == NULL || prefix_len_out == NULL || pad_width_out == NULL || extension_out == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // Template is a file name only, directory separators are rejected
  // This prevents output from escaping the selected output directory
  while (template_text[index] != '\0') {
    if (template_text[index] == '/') {
      return PLAINSIGHT_ERR_ARGS;
    }
    if (template_text[index] == '%') {
      if (saw_percent != 0) {
        // Only one format slot is supported for predictable output
        return PLAINSIGHT_ERR_ARGS;
      }
      saw_percent = 1;
      percent_index = index;
    }
    if (template_text[index] == '.') {
      // The last dot is used as the extension separator
      saw_dot = 1;
      dot_index = index;
    }
    index++;
  }

  if (saw_percent == 0 || saw_dot == 0) {
    return PLAINSIGHT_ERR_ARGS;
  }
  if (dot_index == 0u || dot_index + 1u >= index) {
    return PLAINSIGHT_ERR_ARGS;
  }
  if (percent_index + 2u > index) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // Supported forms: "%u" and "%0<width>u"
  // This is intentionally small so the formatter can be implemented without extra runtime helpers
  index = percent_index + 1u;
  if (template_text[index] == '0') {
    zero_pad = 1;
    index++;
  }

  while (template_text[index] >= '0' && template_text[index] <= '9') {
    unsigned int digit_value = (unsigned int)(template_text[index] - '0');
    if (pad_width > 100000u) {
      // pad_width is bounded so formatting cannot loop for huge counts
      return PLAINSIGHT_ERR_ARGS;
    }
    pad_width = pad_width * 10u + digit_value;
    index++;
  }

  if (template_text[index] != 'u') {
    return PLAINSIGHT_ERR_ARGS;
  }

  // Disallow "%0u" style which is ambiguous and does not provide a width
  if (zero_pad != 0 && pad_width == 0u) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // Default padding is 0 which means no fixed width formatting
  *pad_width_out = pad_width;

  // Extension is everything after the last dot
  *extension_out = template_text + dot_index + 1u;
  if (!plainsight_cli_extension_is_lossless_output(*extension_out)) {
    // Lossless output is required so embedded bits survive the image writer
    return PLAINSIGHT_ERR_UNSUPPORTED;
  }

  // Prefix is bytes before '%'
  *prefix_len_out = percent_index;
  return PLAINSIGHT_OK;
}

static plainsight_error plainsight_cli_write_u32_padded(char *out, size_t out_cap, uint32_t value,
                                                        unsigned int pad_width, size_t *written_out) {
  char reversed_digits[16];
  size_t digit_count = 0u;
  size_t out_index = 0u;

  if (out == NULL || written_out == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  if (value == 0u) {
    reversed_digits[digit_count++] = '0';
  } else {
    while (value > 0u && digit_count < sizeof(reversed_digits)) {
      // Digits are collected in reverse and then copied back out
      reversed_digits[digit_count++] = (char)('0' + (char)(value % 10u));
      value /= 10u;
    }
  }

  if (pad_width > 0u && (size_t)pad_width > digit_count) {
    size_t pad_count = (size_t)pad_width - digit_count;
    if (pad_count >= out_cap) {
      return PLAINSIGHT_ERR_TOO_LARGE;
    }
    while (pad_count > 0u) {
      out[out_index++] = '0';
      pad_count--;
    }
  }

  if (out_index + digit_count >= out_cap) {
    return PLAINSIGHT_ERR_TOO_LARGE;
  }

  while (digit_count > 0u) {
    out[out_index++] = reversed_digits[digit_count - 1u];
    digit_count--;
  }

  *written_out = out_index;
  return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_split_build_default_template(const char *cover_path, char *out,
                                                             size_t out_cap) {
  const char *extension_text = NULL;
  size_t prefix_len = 0u;
  size_t index = 0u;

  if (cover_path == NULL || out == NULL || out_cap == 0u) {
    return PLAINSIGHT_ERR_ARGS;
  }

  extension_text = plainsight_cli_lossless_extension_for_cover(cover_path);
  prefix_len = sizeof(g_template_default_prefix) - 1u;

  // Build "shard_%04u.<ext>" without snprintf
  if (prefix_len + 4u + 3u + 1u + 4u >= out_cap) {
    return PLAINSIGHT_ERR_TOO_LARGE;
  }

  for (index = 0u; index < prefix_len; index++) {
    out[index] = g_template_default_prefix[index];
  }
  out[index++] = '%';
  out[index++] = '0';
  out[index++] = '4';
  out[index++] = 'u';
  out[index++] = '.';

  while (*extension_text != '\0') {
    if (index + 1u >= out_cap) {
      return PLAINSIGHT_ERR_TOO_LARGE;
    }
    out[index++] = *extension_text++;
  }
  out[index] = '\0';

  // Parse after build so the extension policy remains enforced by one validator
  {
    size_t ignored_prefix = 0u;
    unsigned int ignored_width = 0u;
    const char *ignored_extension = NULL;
    if (plainsight_cli_parse_template(out, &ignored_prefix, &ignored_width, &ignored_extension) !=
        PLAINSIGHT_OK) {
      return PLAINSIGHT_ERR_INTERNAL;
    }
  }

  return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_split_format_shard_filename(const char *template_text, uint32_t shard_index,
                                                            char *out, size_t out_cap) {
  size_t prefix_len = 0u;
  unsigned int pad_width = 0u;
  const char *extension_text = NULL;
  size_t index = 0u;
  size_t number_written = 0u;
  size_t suffix_index = 0u;

  if (template_text == NULL || out == NULL || out_cap == 0u) {
    return PLAINSIGHT_ERR_ARGS;
  }

  if (plainsight_cli_parse_template(template_text, &prefix_len, &pad_width, &extension_text) !=
      PLAINSIGHT_OK) {
    // Template parsing errors map to argument errors for consistent CLI UX
    return PLAINSIGHT_ERR_ARGS;
  }

  // Copy prefix bytes up to the format slot
  for (index = 0u; index < prefix_len; index++) {
    if (index + 1u >= out_cap) {
      return PLAINSIGHT_ERR_TOO_LARGE;
    }
    out[index] = template_text[index];
  }

  if (plainsight_cli_write_u32_padded(out + prefix_len, out_cap - prefix_len, shard_index, pad_width,
                                      &number_written) != PLAINSIGHT_OK) {
    // Name overflow is treated as a hard error so output cannot be truncated
    return PLAINSIGHT_ERR_TOO_LARGE;
  }

  index = prefix_len + number_written;

  // Copy suffix bytes after the 'u' and keep extension intact
  suffix_index = prefix_len;
  while (template_text[suffix_index] != '\0' && template_text[suffix_index] != '%') {
    suffix_index++;
  }
  while (template_text[suffix_index] != '\0' && template_text[suffix_index] != 'u') {
    suffix_index++;
  }
  if (template_text[suffix_index] != 'u') {
    return PLAINSIGHT_ERR_ARGS;
  }
  suffix_index++;

  while (template_text[suffix_index] != '\0') {
    if (index + 1u >= out_cap) {
      return PLAINSIGHT_ERR_TOO_LARGE;
    }
    out[index++] = template_text[suffix_index++];
  }
  out[index] = '\0';

  // Ensure the resulting name still has the expected extension
  if (extension_text == NULL || extension_text[0] == '\0') {
    return PLAINSIGHT_ERR_ARGS;
  }

  return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_split_preflight_outputs(const char *output_dir, const char *template_text,
                                                        uint32_t shard_count) {
  uint32_t shard_index = 0u;
  char shard_name[256];
  char shard_path[1024];

  if (output_dir == NULL || template_text == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  for (shard_index = 0u; shard_index < shard_count; shard_index++) {
    // Preflight uses the exact same naming code as the writer loop
    // This is used to avoid leaving partial output sets on error
    if (plainsight_cli_split_format_shard_filename(template_text, shard_index, shard_name,
                                                   sizeof(shard_name)) != PLAINSIGHT_OK) {
      return PLAINSIGHT_ERR_ARGS;
    }
    if (plainsight_cli_join_dir_and_name(output_dir, shard_name, shard_path, sizeof(shard_path)) !=
        PLAINSIGHT_OK) {
      return PLAINSIGHT_ERR_TOO_LARGE;
    }
    {
      int exists_flag = 0;
      plainsight_error exists_rc = plainsight_cli_path_exists(shard_path, &exists_flag);
      if (exists_rc != PLAINSIGHT_OK) {
        return exists_rc;
      }
      if (exists_flag != 0) {
        // Output paths must be new to keep "best effort atomic" behavior simple
        return PLAINSIGHT_ERR_IO;
      }
    }
  }

  return PLAINSIGHT_OK;
}
