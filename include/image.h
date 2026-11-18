#ifndef nector_image_h
#define nector_image_h

#include "rasterization.h"

#if defined(_WIN32)
  #if defined(nector_image_export)
    #define nector_image_api __declspec(dllexport)
  #else
    #define nector_image_api __declspec(dllimport)
  #endif
#else
  #define nector_image_api __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nector_image nector_image;
typedef nector_image* nector_image_handle;

typedef enum nector_image_format {
    nector_image_unknown = 0,
    nector_image_png = 1,
    nector_image_jpeg = 2,
    nector_image_bmp = 3,
    nector_image_svg = 4
} nector_image_format;

nector_image_api nector_status nectorImageLoad(const char* path, nector_image_handle* out_image);
nector_image_api nector_status nectorImageLoadMemory(const uint8_t* data, size_t size, nector_image_handle* out_image);
nector_image_api nector_status nectorImageLoadSvg(const char* utf8_svg, int32_t output_width, int32_t output_height, nector_image_handle* out_image);
nector_image_api nector_status nectorImageCreate(int32_t width, int32_t height, const uint8_t* rgba, size_t pixel_size, nector_image_handle* out_image);
nector_image_api nector_status nectorImageRetain(nector_image_handle image);
nector_image_api void nectorImageRelease(nector_image_handle image);
nector_image_api void nectorImageDestroy(nector_image_handle image);
nector_image_api nector_status nectorImageGetSize(nector_image_handle image, int32_t* out_width, int32_t* out_height);
nector_image_api nector_status nectorImageReadPixels(nector_image_handle image, uint8_t* out_rgba, size_t pixel_size);
nector_image_api nector_status nectorImageDraw(nector_painter_handle painter, nector_image_handle image, nector_rect destination, nector_rect source_rect, float opacity);
nector_image_api nector_status nectorImageSavePng(nector_image_handle image, const char* path);
nector_image_api nector_status nectorImageSaveBmp(nector_image_handle image, const char* path);

#ifdef __cplusplus
}
#endif
#endif
