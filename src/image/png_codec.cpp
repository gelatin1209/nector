#include "image_internal.h"

#include <png.h>

#include <cstring>
#include <string>
#include <vector>

namespace nector_image_internal {
namespace {

struct PngImageGuard {
    png_image image{};
    ~PngImageGuard() { png_image_free(&image); }
};

nector_status pngFailure(const png_image& image, nector_status fallback, const char* prefix) {
    const std::string message = std::string(prefix) + (image.message[0] == '\0' ? "unknown libpng error" : image.message);
    return nectorFail(fallback, message.c_str());
}

}

nector_status decodePng(const uint8_t* data, size_t size, DecodedImage* out_image) {
    if (data == nullptr || out_image == nullptr) return nectorFail(nector_invalid_argument, "PNG input is null");
    PngImageGuard png;
    png.image.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_memory(&png.image, data, size) == 0) {
        return pngFailure(png.image, nector_invalid_format, "PNG decode failed: ");
    }
    if (png.image.width == 0 || png.image.height == 0 ||
        png.image.width > static_cast<png_uint_32>(std::numeric_limits<int32_t>::max()) ||
        png.image.height > static_cast<png_uint_32>(std::numeric_limits<int32_t>::max())) {
        return nectorFail(nector_limit_exceeded, "PNG dimensions are too large");
    }
    const int32_t width = static_cast<int32_t>(png.image.width);
    const int32_t height = static_cast<int32_t>(png.image.height);
    size_t buffer_size = 0;
    if (!pixelBufferSize(width, height, &buffer_size)) return nectorFail(nector_limit_exceeded, "PNG image is too large");
    if (width > std::numeric_limits<png_int_32>::max() / 4) {
        return nectorFail(nector_limit_exceeded, "PNG row stride is too large");
    }
    DecodedImage decoded;
    decoded.width = width;
    decoded.height = height;
    decoded.pixels.resize(buffer_size);
    png.image.format = PNG_FORMAT_RGBA;
    if (png_image_finish_read(&png.image, nullptr, decoded.pixels.data(), width * 4, nullptr) == 0) {
        return pngFailure(png.image, nector_invalid_format, "PNG decode failed: ");
    }
    *out_image = std::move(decoded);
    return nector_ok;
}

}

nector_status nectorImageSavePng(nector_image_handle image, const char* path) {
    if (path == nullptr || path[0] == '\0') return nectorFail(nector_invalid_argument, "path is null or empty");
    if (image == nullptr) return nectorFail(nector_invalid_argument, "image is null");
    nector_api_try {
        int32_t width = 0;
        int32_t height = 0;
        nector_status status = nectorSurfaceGetSize(image->surface, &width, &height);
        if (status != nector_ok) return status;
        size_t buffer_size = 0;
        if (!nector_image_internal::pixelBufferSize(width, height, &buffer_size) ||
            width > std::numeric_limits<png_int_32>::max() / 4) {
            return nectorFail(nector_limit_exceeded, "PNG output dimensions are too large");
        }
        std::vector<uint8_t> pixels(buffer_size);
        status = nectorSurfaceReadPixels(image->surface, pixels.data(), pixels.size());
        if (status != nector_ok) return status;
        nector_image_internal::PngImageGuard png;
        png.image.version = PNG_IMAGE_VERSION;
        png.image.width = static_cast<png_uint_32>(width);
        png.image.height = static_cast<png_uint_32>(height);
        png.image.format = PNG_FORMAT_RGBA;
        if (png_image_write_to_file(&png.image, path, 0, pixels.data(), width * 4, nullptr) == 0) {
            return nector_image_internal::pngFailure(png.image, nector_io_error, "PNG save failed: ");
        }
        return nector_ok;
    } nector_api_catch
}
