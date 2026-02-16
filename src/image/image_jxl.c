#include <stddef.h>
#include <stdint.h>

#include "../../include/image/image.h"
#include "../../include/image/image_jxl.h"

#if PLAINSIGHT_HAS_LIBJXL

#include <jxl/decode.h>
#include <jxl/encode.h>
#include <jxl/thread_parallel_runner.h>

#include "../../include/io.h"

// Shared bounded buffers keep decode and encode heap-free in this layer
// These buffers are process-global and therefore not reentrant
static uint8_t g_jxl_input[PLAINSIGHT_MAX_IMAGE_FILE_BYTES];
static uint8_t g_jxl_output[PLAINSIGHT_MAX_IMAGE_FILE_BYTES];

plainsight_error plainsight_image_jxl_read(const char *path, plainsight_image *image) {
    size_t input_len = 0u;
    JxlDecoder *decoder_state = NULL;
    JxlDecoderStatus decode_status = JXL_DEC_ERROR;
    JxlPixelFormat pixel_format;
    JxlBasicInfo image_info;
    size_t output_buffer_size = 0u;
    uint32_t decoded_width = 0u;
    uint32_t decoded_height = 0u;
    size_t decoded_data_len = 0u;
    size_t expected_output_len = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
    int saw_image = 0;
    int saw_basic_info = 0;
    void *parallel_runner = NULL;

    if (path == NULL || image == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Caller provides pixel storage so this backend stays heap-free for full frames
    if (image->pixels == NULL || image->pixels_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Decode starts from a bounded in-memory buffer
    result_code = plainsight_io_read_file(path, g_jxl_input, sizeof(g_jxl_input), &input_len);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    decoder_state = JxlDecoderCreate(NULL);
    if (decoder_state == NULL) {
        return PLAINSIGHT_ERR_INTERNAL;
    }

    // Thread runner is optional in the API but improves decode speed
    parallel_runner = JxlThreadParallelRunnerCreate(NULL, JxlThreadParallelRunnerDefaultNumWorkerThreads());
    if (parallel_runner == NULL) {
        result_code = PLAINSIGHT_ERR_INTERNAL;
        goto cleanup;
    }

    if (JxlDecoderSetParallelRunner(decoder_state, JxlThreadParallelRunner, parallel_runner) != JXL_DEC_SUCCESS) {
        result_code = PLAINSIGHT_ERR_INTERNAL;
        goto cleanup;
    }

    if (JxlDecoderSubscribeEvents(decoder_state, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS) {
        result_code = PLAINSIGHT_ERR_INTERNAL;
        goto cleanup;
    }

    if (JxlDecoderSetInput(decoder_state, g_jxl_input, input_len) != JXL_DEC_SUCCESS) {
        result_code = PLAINSIGHT_ERR_BAD_FORMAT;
        goto cleanup;
    }
    JxlDecoderCloseInput(decoder_state);

    pixel_format.num_channels = 3u;
    pixel_format.data_type = JXL_TYPE_UINT8;
    pixel_format.endianness = JXL_NATIVE_ENDIAN;
    pixel_format.align = 0u;

    while (1) {
        decode_status = JxlDecoderProcessInput(decoder_state);

        if (decode_status == JXL_DEC_ERROR) {
            result_code = PLAINSIGHT_ERR_BAD_FORMAT;
            goto cleanup;
        }

        if (decode_status == JXL_DEC_NEED_MORE_INPUT) {
            result_code = PLAINSIGHT_ERR_BAD_FORMAT;
            goto cleanup;
        }

        if (decode_status == JXL_DEC_BASIC_INFO) {
            if (JxlDecoderGetBasicInfo(decoder_state, &image_info) != JXL_DEC_SUCCESS) {
                result_code = PLAINSIGHT_ERR_BAD_FORMAT;
                goto cleanup;
            }

            if (image_info.xsize == 0u ||
                image_info.ysize == 0u ||
                image_info.xsize > PLAINSIGHT_MAX_IMAGE_DIMENSION ||
                image_info.ysize > PLAINSIGHT_MAX_IMAGE_DIMENSION) {
                result_code = PLAINSIGHT_ERR_TOO_LARGE;
                goto cleanup;
            }

            // Keep 8-bit integer path only to avoid precision pitfalls
            if (image_info.bits_per_sample != 8u || image_info.exponent_bits_per_sample != 0u) {
                result_code = PLAINSIGHT_ERR_UNSUPPORTED;
                goto cleanup;
            }

            decoded_width = image_info.xsize;
            decoded_height = image_info.ysize;
            saw_basic_info = 1;
            continue;
        }

        if (decode_status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            if (saw_basic_info == 0) {
                result_code = PLAINSIGHT_ERR_BAD_FORMAT;
                goto cleanup;
            }

            // Ask decoder for exact output bytes required for this frame
            if (JxlDecoderImageOutBufferSize(decoder_state, &pixel_format, &output_buffer_size) != JXL_DEC_SUCCESS) {
                result_code = PLAINSIGHT_ERR_BAD_FORMAT;
                goto cleanup;
            }

            if (output_buffer_size > PLAINSIGHT_MAX_IMAGE_BYTES) {
                result_code = PLAINSIGHT_ERR_TOO_LARGE;
                goto cleanup;
            }

            // pixels_cap check keeps decoder writes inside bound storage
            if (output_buffer_size > image->pixels_cap) {
                result_code = PLAINSIGHT_ERR_TOO_LARGE;
                goto cleanup;
            }

            // This backend expects tightly packed RGB8 output without row padding
            if (decoded_width > 0u && decoded_height > 0u) {
                if ((uint64_t)decoded_width * (uint64_t)decoded_height > (uint64_t)(SIZE_MAX / 3u)) {
                    result_code = PLAINSIGHT_ERR_TOO_LARGE;
                    goto cleanup;
                }
                expected_output_len = (size_t)decoded_width * (size_t)decoded_height * 3u;
                if (output_buffer_size != expected_output_len) {
                    result_code = PLAINSIGHT_ERR_BAD_FORMAT;
                    goto cleanup;
                }
            }

            if (JxlDecoderSetImageOutBuffer(decoder_state,
                                            &pixel_format,
                                            image->pixels,
                                            output_buffer_size) != JXL_DEC_SUCCESS) {
                result_code = PLAINSIGHT_ERR_BAD_FORMAT;
                goto cleanup;
            }

            decoded_data_len = output_buffer_size;
            continue;
        }

        if (decode_status == JXL_DEC_FULL_IMAGE) {
            saw_image = 1;
            continue;
        }

        if (decode_status == JXL_DEC_SUCCESS) {
            if (saw_image == 0 || saw_basic_info == 0 || decoded_data_len == 0u) {
                result_code = PLAINSIGHT_ERR_BAD_FORMAT;
                goto cleanup;
            }

            // Commit decoded metadata only after all decode steps succeeded
            image->width = decoded_width;
            image->height = decoded_height;
            image->channels = 3u;
            image->data_len = decoded_data_len;
            result_code = PLAINSIGHT_OK;
            goto cleanup;
        }
    }

cleanup:
    if (parallel_runner != NULL) {
        JxlThreadParallelRunnerDestroy(parallel_runner);
    }
    if (decoder_state != NULL) {
        JxlDecoderDestroy(decoder_state);
    }
    return result_code;
}

plainsight_error plainsight_image_jxl_write(const char *path, const plainsight_image *image) {
    uint8_t *next_out = g_jxl_output;
    size_t avail_out = sizeof(g_jxl_output);
    JxlEncoder *encoder_state = NULL;
    JxlEncoderFrameSettings *encoder_frame_settings = NULL;
    JxlBasicInfo image_info;
    JxlColorEncoding color_encoding;
    JxlPixelFormat pixel_format;
    JxlEncoderStatus encode_status = JXL_ENC_ERROR;
    size_t output_len = 0u;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
    void *parallel_runner = NULL;

    if (path == NULL || image == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Writer reads RGB bytes from caller-provided storage only
    if (image->pixels == NULL || image->pixels_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (image->width == 0u || image->height == 0u || image->channels != 3u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if ((uint64_t)image->width * (uint64_t)image->height * 3u != (uint64_t)image->data_len) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // pixels_cap mismatch indicates corrupted caller state or invalid binding
    if ((uint64_t)image->data_len > (uint64_t)image->pixels_cap) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    encoder_state = JxlEncoderCreate(NULL);
    if (encoder_state == NULL) {
        return PLAINSIGHT_ERR_INTERNAL;
    }

    parallel_runner = JxlThreadParallelRunnerCreate(NULL, JxlThreadParallelRunnerDefaultNumWorkerThreads());
    if (parallel_runner == NULL) {
        result_code = PLAINSIGHT_ERR_INTERNAL;
        goto cleanup;
    }

    if (JxlEncoderSetParallelRunner(encoder_state, JxlThreadParallelRunner, parallel_runner) != JXL_ENC_SUCCESS) {
        result_code = PLAINSIGHT_ERR_INTERNAL;
        goto cleanup;
    }

    encoder_frame_settings = JxlEncoderFrameSettingsCreate(encoder_state, NULL);
    if (encoder_frame_settings == NULL) {
        result_code = PLAINSIGHT_ERR_INTERNAL;
        goto cleanup;
    }

    JxlEncoderInitBasicInfo(&image_info);
    image_info.xsize = image->width;
    image_info.ysize = image->height;
    image_info.bits_per_sample = 8u;
    image_info.exponent_bits_per_sample = 0u;
    image_info.num_color_channels = 3u;
    image_info.num_extra_channels = 0u;
    image_info.alpha_bits = 0u;
    // Output always uses the explicit sRGB profile configured below
    image_info.uses_original_profile = JXL_FALSE;

    if (JxlEncoderSetBasicInfo(encoder_state, &image_info) != JXL_ENC_SUCCESS) {
        result_code = PLAINSIGHT_ERR_INTERNAL;
        goto cleanup;
    }

    JxlColorEncodingSetToSRGB(&color_encoding, JXL_FALSE);
    if (JxlEncoderSetColorEncoding(encoder_state, &color_encoding) != JXL_ENC_SUCCESS) {
        result_code = PLAINSIGHT_ERR_INTERNAL;
        goto cleanup;
    }

    pixel_format.num_channels = 3u;
    pixel_format.data_type = JXL_TYPE_UINT8;
    pixel_format.endianness = JXL_NATIVE_ENDIAN;
    pixel_format.align = 0u;

    // Force lossless settings so embedded bits survive write/read roundtrips
    // Dedicated lossless API is preferred over relying on distance alone
    if (JxlEncoderSetFrameLossless(encoder_frame_settings, JXL_TRUE) != JXL_ENC_SUCCESS) {
        result_code = PLAINSIGHT_ERR_INTERNAL;
        goto cleanup;
    }

    if (JxlEncoderFrameSettingsSetOption(encoder_frame_settings, JXL_ENC_FRAME_SETTING_MODULAR, 1) != JXL_ENC_SUCCESS) {
        result_code = PLAINSIGHT_ERR_INTERNAL;
        goto cleanup;
    }

    if (JxlEncoderSetFrameDistance(encoder_frame_settings, 0.0f) != JXL_ENC_SUCCESS) {
        result_code = PLAINSIGHT_ERR_INTERNAL;
        goto cleanup;
    }

    if (JxlEncoderAddImageFrame(encoder_frame_settings,
                                &pixel_format,
                                image->pixels,
                                image->data_len) != JXL_ENC_SUCCESS) {
        result_code = PLAINSIGHT_ERR_INTERNAL;
        goto cleanup;
    }

    JxlEncoderCloseInput(encoder_state);

    while (1) {
        encode_status = JxlEncoderProcessOutput(encoder_state, &next_out, &avail_out);
        if (encode_status == JXL_ENC_ERROR) {
            result_code = PLAINSIGHT_ERR_INTERNAL;
            goto cleanup;
        }
        if (encode_status == JXL_ENC_NEED_MORE_OUTPUT) {
            result_code = PLAINSIGHT_ERR_TOO_LARGE;
            goto cleanup;
        }
        if (encode_status == JXL_ENC_SUCCESS) {
            break;
        }
    }

    output_len = sizeof(g_jxl_output) - avail_out;
    result_code = plainsight_io_write_file(path, g_jxl_output, output_len);

cleanup:
    if (parallel_runner != NULL) {
        JxlThreadParallelRunnerDestroy(parallel_runner);
    }
    if (encoder_state != NULL) {
        JxlEncoderDestroy(encoder_state);
    }
    return result_code;
}

#else

plainsight_error plainsight_image_jxl_read(const char *path, plainsight_image *image) {
    (void)path;
    (void)image;
    return PLAINSIGHT_ERR_UNSUPPORTED;
}

plainsight_error plainsight_image_jxl_write(const char *path, const plainsight_image *image) {
    (void)path;
    (void)image;
    return PLAINSIGHT_ERR_UNSUPPORTED;
}

#endif
