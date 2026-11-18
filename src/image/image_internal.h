#ifndef nector_image_internal_h
#define nector_image_internal_h

#include "image.h"
#include "../core/internal.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

struct nector_image {
    nector_surface_handle surface = nullptr;
    nector_image_format format = nector_image_unknown;
};

namespace nector_image_internal {

constexpr size_t max_image_pixels = 100000000;
constexpr size_t max_encoded_size = 256u * 1024u * 1024u;
constexpr size_t max_svg_size = 16u * 1024u * 1024u;

struct DecodedImage {
    int32_t width = 0;
    int32_t height = 0;
    std::vector<uint8_t> pixels;
};

struct SurfaceDeleter {
    void operator()(nector_surface_handle surface) const { nectorDestroySurface(surface); }
};
using SurfacePtr = std::unique_ptr<nector_surface, SurfaceDeleter>;

bool pixelBufferSize(int32_t width, int32_t height, size_t* out_size);
nector_status makeImage(int32_t width, int32_t height, const uint8_t* rgba, size_t pixel_size,
                        nector_image_format format, nector_image_handle* out_image);

nector_status decodePng(const uint8_t* data, size_t size, DecodedImage* out_image);
nector_status decodeJpeg(const uint8_t* data, size_t size, DecodedImage* out_image);
nector_status decodeBmp(const uint8_t* data, size_t size, DecodedImage* out_image);
nector_status decodeSvg(const uint8_t* data, size_t size, int32_t output_width, int32_t output_height,
                        DecodedImage* out_image);

}

#endif
