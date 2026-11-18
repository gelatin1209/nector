#include "image_internal.h"

#include <jpeglib.h>

#include <csetjmp>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

namespace nector_image_internal {
namespace {

struct JpegErrorManager {
    jpeg_error_mgr base;
    jmp_buf jump_buffer;
    char message[JMSG_LENGTH_MAX]{};
};

struct FreeDeleter {
    void operator()(uint8_t* memory) const { std::free(memory); }
};

void jpegErrorExit(j_common_ptr info) {
    auto* error = reinterpret_cast<JpegErrorManager*>(info->err);
    (*info->err->format_message)(info, error->message);
    longjmp(error->jump_buffer, 1);
}

uint8_t multiplyBytes(uint8_t first, uint8_t second) {
    return static_cast<uint8_t>((static_cast<unsigned int>(first) * second + 127u) / 255u);
}

nector_status decodeRaw(const uint8_t* data, size_t size, int32_t* out_width, int32_t* out_height,
                        uint8_t** out_pixels) {
    jpeg_decompress_struct info{};
    JpegErrorManager error{};
    info.err = jpeg_std_error(&error.base);
    error.base.error_exit = jpegErrorExit;
    volatile bool created = false;
    uint8_t* volatile pixels = nullptr;
    JSAMPLE* volatile row = nullptr;
    if (setjmp(error.jump_buffer) != 0) {
        std::free(pixels);
        std::free(row);
        if (created) jpeg_destroy_decompress(&info);
        const std::string message = std::string("JPEG decode failed: ") +
                                    (error.message[0] == '\0' ? "unknown libjpeg error" : error.message);
        return nectorFail(nector_invalid_format, message.c_str());
    }
    jpeg_create_decompress(&info);
    created = true;
    jpeg_mem_src(&info, data, size);
    if (jpeg_read_header(&info, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&info);
        return nectorFail(nector_invalid_format, "JPEG has no image header");
    }
    const bool cmyk = info.jpeg_color_space == JCS_CMYK || info.jpeg_color_space == JCS_YCCK;
    info.out_color_space = cmyk ? JCS_CMYK : JCS_RGB;
    if (jpeg_start_decompress(&info) == FALSE || info.output_width == 0 || info.output_height == 0 ||
        info.output_width > static_cast<JDIMENSION>(std::numeric_limits<int32_t>::max()) ||
        info.output_height > static_cast<JDIMENSION>(std::numeric_limits<int32_t>::max())) {
        jpeg_destroy_decompress(&info);
        return nectorFail(nector_limit_exceeded, "JPEG output dimensions are invalid or too large");
    }
    const int32_t width = static_cast<int32_t>(info.output_width);
    const int32_t height = static_cast<int32_t>(info.output_height);
    size_t buffer_size = 0;
    if (!pixelBufferSize(width, height, &buffer_size)) {
        jpeg_destroy_decompress(&info);
        return nectorFail(nector_limit_exceeded, "JPEG image is too large");
    }
    const int components = info.output_components;
    if ((!cmyk && components != 3) || (cmyk && components != 4)) {
        jpeg_destroy_decompress(&info);
        return nectorFail(nector_unsupported, "JPEG output color layout is unsupported");
    }
    if (info.output_width > std::numeric_limits<size_t>::max() / static_cast<size_t>(components)) {
        jpeg_destroy_decompress(&info);
        return nectorFail(nector_limit_exceeded, "JPEG scanline is too large");
    }
    const size_t row_size = static_cast<size_t>(info.output_width) * static_cast<size_t>(components);
    pixels = static_cast<uint8_t*>(std::malloc(buffer_size));
    row = static_cast<JSAMPLE*>(std::malloc(row_size));
    if (pixels == nullptr || row == nullptr) {
        std::free(pixels);
        std::free(row);
        jpeg_destroy_decompress(&info);
        return nectorFail(nector_out_of_memory, "cannot allocate JPEG decode buffers");
    }
    while (info.output_scanline < info.output_height) {
        JSAMPROW rows[1] = {row};
        const JDIMENSION previous = info.output_scanline;
        if (jpeg_read_scanlines(&info, rows, 1) != 1 || info.output_scanline != previous + 1) {
            std::free(pixels);
            std::free(row);
            jpeg_destroy_decompress(&info);
            return nectorFail(nector_invalid_format, "JPEG scanline decoding failed");
        }
        uint8_t* destination = pixels + static_cast<size_t>(previous) * width * 4;
        for (int32_t x = 0; x < width; ++x) {
            const JSAMPLE* source = row + static_cast<size_t>(x) * components;
            if (!cmyk) {
                destination[0] = source[0];
                destination[1] = source[1];
                destination[2] = source[2];
            } else if (info.saw_Adobe_marker) {
                destination[0] = multiplyBytes(source[0], source[3]);
                destination[1] = multiplyBytes(source[1], source[3]);
                destination[2] = multiplyBytes(source[2], source[3]);
            } else {
                destination[0] = multiplyBytes(static_cast<uint8_t>(255 - source[0]), static_cast<uint8_t>(255 - source[3]));
                destination[1] = multiplyBytes(static_cast<uint8_t>(255 - source[1]), static_cast<uint8_t>(255 - source[3]));
                destination[2] = multiplyBytes(static_cast<uint8_t>(255 - source[2]), static_cast<uint8_t>(255 - source[3]));
            }
            destination[3] = 255;
            destination += 4;
        }
    }
    if (jpeg_finish_decompress(&info) == FALSE) {
        std::free(pixels);
        std::free(row);
        jpeg_destroy_decompress(&info);
        return nectorFail(nector_invalid_format, "JPEG stream did not finish cleanly");
    }
    jpeg_destroy_decompress(&info);
    std::free(row);
    *out_width = width;
    *out_height = height;
    *out_pixels = pixels;
    return nector_ok;
}

}

nector_status decodeJpeg(const uint8_t* data, size_t size, DecodedImage* out_image) {
    if (data == nullptr || out_image == nullptr) return nectorFail(nector_invalid_argument, "JPEG input is null");
    int32_t width = 0;
    int32_t height = 0;
    uint8_t* raw_pixels = nullptr;
    const nector_status status = decodeRaw(data, size, &width, &height, &raw_pixels);
    if (status != nector_ok) return status;
    std::unique_ptr<uint8_t, FreeDeleter> guard(raw_pixels);
    size_t buffer_size = 0;
    if (!pixelBufferSize(width, height, &buffer_size)) return nectorFail(nector_invalid_state, "JPEG returned invalid dimensions");
    DecodedImage decoded;
    decoded.width = width;
    decoded.height = height;
    decoded.pixels.assign(raw_pixels, raw_pixels + buffer_size);
    *out_image = std::move(decoded);
    return nector_ok;
}

}
