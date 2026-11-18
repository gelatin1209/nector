#include "image_internal.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>

namespace {

struct ImageAllocation {
    nector_image image;
    std::atomic<uint32_t> references{1};
};

ImageAllocation* imageAllocation(nector_image_handle image) {
    return reinterpret_cast<ImageAllocation*>(image);
}

}

namespace nector_image_internal {

bool pixelBufferSize(int32_t width, int32_t height, size_t* out_size) {
    if (out_size == nullptr || width <= 0 || height <= 0) return false;
    const size_t w = static_cast<size_t>(width);
    const size_t h = static_cast<size_t>(height);
    if (w > max_image_pixels / h) return false;
    const size_t pixels = w * h;
    if (pixels > std::numeric_limits<size_t>::max() / 4) return false;
    *out_size = pixels * 4;
    return true;
}

nector_status makeImage(int32_t width, int32_t height, const uint8_t* rgba, size_t pixel_size,
                        nector_image_format format, nector_image_handle* out_image) {
    if (out_image == nullptr) return nectorFail(nector_invalid_argument, "out_image is null");
    *out_image = nullptr;
    size_t required = 0;
    if (!pixelBufferSize(width, height, &required)) {
        return nectorFail(nector_limit_exceeded, "image dimensions are invalid or too large");
    }
    if (rgba == nullptr || pixel_size < required) {
        return nectorFail(nector_invalid_argument, "pixel buffer is null or too small");
    }
    SurfacePtr surface;
    nector_surface_handle raw_surface = nullptr;
    nector_status status = nectorCreateSurfaceFromPixels(width, height, rgba, required, &raw_surface);
    if (status != nector_ok) return status;
    surface.reset(raw_surface);
    std::unique_ptr<ImageAllocation> allocation(new ImageAllocation);
    allocation->image.surface = surface.release();
    allocation->image.format = format;
    *out_image = &allocation->image;
    allocation.release();
    return nector_ok;
}

nector_image_format sniffFormat(const uint8_t* data, size_t size) {
    static const uint8_t png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size >= sizeof(png_signature) && std::memcmp(data, png_signature, sizeof(png_signature)) == 0) {
        return nector_image_png;
    }
    if (size >= 3 && data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff) return nector_image_jpeg;
    if (size >= 2 && data[0] == 'B' && data[1] == 'M') return nector_image_bmp;
    size_t offset = 0;
    if (size >= 3 && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf) offset = 3;
    while (offset < size && (data[offset] == ' ' || data[offset] == '\t' || data[offset] == '\r' || data[offset] == '\n')) {
        ++offset;
    }
    if (offset < size && data[offset] == '<') return nector_image_svg;
    return nector_image_unknown;
}

}

namespace {
nector_status loadDecoded(const uint8_t* data, size_t size, nector_image_format format,
                          int32_t output_width, int32_t output_height, nector_image_handle* out_image) {
    nector_image_internal::DecodedImage decoded;
    nector_status status = nector_invalid_format;
    switch (format) {
        case nector_image_png:
            status = nector_image_internal::decodePng(data, size, &decoded);
            break;
        case nector_image_jpeg:
            status = nector_image_internal::decodeJpeg(data, size, &decoded);
            break;
        case nector_image_bmp:
            status = nector_image_internal::decodeBmp(data, size, &decoded);
            break;
        case nector_image_svg:
            status = nector_image_internal::decodeSvg(data, size, output_width, output_height, &decoded);
            break;
        default:
            return nectorFail(nector_invalid_format, "unrecognized image format");
    }
    if (status != nector_ok) return status;
    return nector_image_internal::makeImage(decoded.width, decoded.height, decoded.pixels.data(), decoded.pixels.size(),
                                            format, out_image);
}
}

nector_status nectorImageLoad(const char* path, nector_image_handle* out_image) {
    if (out_image == nullptr) return nectorFail(nector_invalid_argument, "out_image is null");
    *out_image = nullptr;
    if (path == nullptr || path[0] == '\0') return nectorFail(nector_invalid_argument, "path is null or empty");
    nector_api_try {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream) return nectorFail(nector_io_error, "cannot open image file");
        const std::streamoff end = stream.tellg();
        if (end <= 0) return nectorFail(nector_invalid_format, "image file is empty");
        const auto unsigned_end = static_cast<uintmax_t>(end);
        if (unsigned_end > nector_image_internal::max_encoded_size ||
            unsigned_end > static_cast<uintmax_t>(std::numeric_limits<size_t>::max())) {
            return nectorFail(nector_limit_exceeded, "encoded image is too large");
        }
        std::vector<uint8_t> data(static_cast<size_t>(unsigned_end));
        stream.seekg(0, std::ios::beg);
        if (!stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
            return nectorFail(nector_io_error, "cannot read image file");
        }
        const nector_image_format format = nector_image_internal::sniffFormat(data.data(), data.size());
        return loadDecoded(data.data(), data.size(), format, 0, 0, out_image);
    } nector_api_catch
}

nector_status nectorImageLoadMemory(const uint8_t* data, size_t size, nector_image_handle* out_image) {
    if (out_image == nullptr) return nectorFail(nector_invalid_argument, "out_image is null");
    *out_image = nullptr;
    if (data == nullptr || size == 0) return nectorFail(nector_invalid_argument, "image data is null or empty");
    if (size > nector_image_internal::max_encoded_size) {
        return nectorFail(nector_limit_exceeded, "encoded image is too large");
    }
    nector_api_try {
        return loadDecoded(data, size, nector_image_internal::sniffFormat(data, size), 0, 0, out_image);
    } nector_api_catch
}

nector_status nectorImageLoadSvg(const char* utf8_svg, int32_t output_width, int32_t output_height,
                                  nector_image_handle* out_image) {
    if (out_image == nullptr) return nectorFail(nector_invalid_argument, "out_image is null");
    *out_image = nullptr;
    if (utf8_svg == nullptr) return nectorFail(nector_invalid_argument, "SVG text is null");
    if (output_width < 0 || output_height < 0 || (output_width == 0) != (output_height == 0)) {
        return nectorFail(nector_invalid_argument, "both SVG output dimensions must be positive or zero");
    }
    size_t size = 0;
    while (size <= nector_image_internal::max_svg_size && utf8_svg[size] != '\0') ++size;
    if (size == 0) return nectorFail(nector_invalid_argument, "SVG text is empty");
    if (size > nector_image_internal::max_svg_size) return nectorFail(nector_limit_exceeded, "SVG text is too large");
    nector_api_try {
        const nector_status utf8_status = nectorCoreValidateUtf8(utf8_svg, size);
        if (utf8_status != nector_ok) return utf8_status;
        return loadDecoded(reinterpret_cast<const uint8_t*>(utf8_svg), size, nector_image_svg,
                           output_width, output_height, out_image);
    } nector_api_catch
}

nector_status nectorImageCreate(int32_t width, int32_t height, const uint8_t* rgba, size_t pixel_size,
                                 nector_image_handle* out_image) {
    nector_api_try {
        return nector_image_internal::makeImage(width, height, rgba, pixel_size, nector_image_unknown, out_image);
    } nector_api_catch
}

nector_status nectorImageRetain(nector_image_handle image) {
    if (image == nullptr) return nectorFail(nector_invalid_argument, "image is null");
    uint32_t references = imageAllocation(image)->references.load(std::memory_order_relaxed);
    do {
        if (references == std::numeric_limits<uint32_t>::max()) {
            return nectorFail(nector_limit_exceeded, "image reference count limit exceeded");
        }
    } while (!imageAllocation(image)->references.compare_exchange_weak(
        references, references + 1, std::memory_order_relaxed, std::memory_order_relaxed));
    return nector_ok;
}

void nectorImageRelease(nector_image_handle image) {
    if (image == nullptr) return;
    ImageAllocation* allocation = imageAllocation(image);
    if (allocation->references.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
    nectorDestroySurface(image->surface);
    delete allocation;
}

void nectorImageDestroy(nector_image_handle image) {
    nectorImageRelease(image);
}

nector_status nectorImageGetSize(nector_image_handle image, int32_t* out_width, int32_t* out_height) {
    if (image == nullptr) return nectorFail(nector_invalid_argument, "image is null");
    return nectorSurfaceGetSize(image->surface, out_width, out_height);
}

nector_status nectorImageReadPixels(nector_image_handle image, uint8_t* out_rgba, size_t pixel_size) {
    if (image == nullptr) return nectorFail(nector_invalid_argument, "image is null");
    return nectorSurfaceReadPixels(image->surface, out_rgba, pixel_size);
}

nector_status nectorImageDraw(nector_painter_handle painter, nector_image_handle image, nector_rect destination,
                               nector_rect source_rect, float opacity) {
    if (image == nullptr) return nectorFail(nector_invalid_argument, "image is null");
    return nectorPainterDrawSurface(painter, image->surface, destination, source_rect, opacity);
}

namespace {
nector_status savePixels(nector_image_handle image, std::vector<uint8_t>* pixels, int32_t* width, int32_t* height) {
    if (image == nullptr || pixels == nullptr || width == nullptr || height == nullptr) {
        return nectorFail(nector_invalid_argument, "image or save output is null");
    }
    nector_status status = nectorSurfaceGetSize(image->surface, width, height);
    if (status != nector_ok) return status;
    size_t size = 0;
    if (!nector_image_internal::pixelBufferSize(*width, *height, &size)) {
        return nectorFail(nector_invalid_state, "stored image dimensions are invalid");
    }
    pixels->resize(size);
    return nectorSurfaceReadPixels(image->surface, pixels->data(), pixels->size());
}
}

nector_status nectorImageSaveBmp(nector_image_handle image, const char* path) {
    if (path == nullptr || path[0] == '\0') return nectorFail(nector_invalid_argument, "path is null or empty");
    nector_api_try {
        std::vector<uint8_t> pixels;
        int32_t width = 0;
        int32_t height = 0;
        nector_status status = savePixels(image, &pixels, &width, &height);
        if (status != nector_ok) return status;
        const uint64_t row_size = static_cast<uint64_t>(static_cast<uint32_t>(width)) * 3u;
        const uint64_t stride = (row_size + 3u) & ~uint64_t{3};
        const uint64_t data_size = stride * static_cast<uint32_t>(height);
        const uint64_t file_size = 54u + data_size;
        if (file_size > std::numeric_limits<uint32_t>::max()) {
            return nectorFail(nector_limit_exceeded, "BMP output is too large");
        }
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream) return nectorFail(nector_io_error, "cannot open BMP output file");
        uint8_t header[54]{};
        auto put16 = [&header](size_t offset, uint16_t value) {
            header[offset] = static_cast<uint8_t>(value);
            header[offset + 1] = static_cast<uint8_t>(value >> 8);
        };
        auto put32 = [&header](size_t offset, uint32_t value) {
            for (size_t index = 0; index < 4; ++index) header[offset + index] = static_cast<uint8_t>(value >> (index * 8));
        };
        header[0] = 'B';
        header[1] = 'M';
        put32(2, static_cast<uint32_t>(file_size));
        put32(10, 54);
        put32(14, 40);
        put32(18, static_cast<uint32_t>(width));
        put32(22, static_cast<uint32_t>(height));
        put16(26, 1);
        put16(28, 24);
        put32(34, static_cast<uint32_t>(data_size));
        stream.write(reinterpret_cast<const char*>(header), sizeof(header));
        std::vector<uint8_t> row(static_cast<size_t>(stride), 0);
        for (int32_t y = height - 1; y >= 0; --y) {
            const size_t source_offset = static_cast<size_t>(y) * static_cast<size_t>(width) * 4;
            for (int32_t x = 0; x < width; ++x) {
                const uint8_t* source = pixels.data() + source_offset + static_cast<size_t>(x) * 4;
                uint8_t* destination = row.data() + static_cast<size_t>(x) * 3;
                destination[0] = source[2];
                destination[1] = source[1];
                destination[2] = source[0];
            }
            stream.write(reinterpret_cast<const char*>(row.data()), static_cast<std::streamsize>(row.size()));
        }
        stream.close();
        if (!stream) return nectorFail(nector_io_error, "cannot write BMP output file");
        return nector_ok;
    } nector_api_catch
}
