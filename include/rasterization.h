#ifndef nector_rasterization_h
#define nector_rasterization_h

#include "core.h"

#if defined(_WIN32)
  #if defined(nector_rasterization_export)
    #define nector_rasterization_api __declspec(dllexport)
  #else
    #define nector_rasterization_api __declspec(dllimport)
  #endif
#else
  #define nector_rasterization_api __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nector_surface nector_surface;
typedef struct nector_painter nector_painter;
typedef struct nector_path nector_path;
typedef nector_surface* nector_surface_handle;
typedef nector_painter* nector_painter_handle;
typedef nector_path* nector_path_handle;

typedef enum nector_fill_rule {
    nector_fill_nonzero = 0,
    nector_fill_even_odd = 1
} nector_fill_rule;

typedef enum nector_line_cap {
    nector_cap_butt = 0,
    nector_cap_round = 1,
    nector_cap_square = 2
} nector_line_cap;

typedef enum nector_line_join {
    nector_join_miter = 0,
    nector_join_round = 1,
    nector_join_bevel = 2
} nector_line_join;

typedef struct nector_stroke_style {
    float width;
    nector_line_cap cap;
    nector_line_join join;
    float miter_limit;
    float dash_offset;
    const float* dash_pattern;
    size_t dash_count;
} nector_stroke_style;

nector_rasterization_api nector_status nectorCreateSurface(int32_t width, int32_t height, nector_surface_handle* out_surface);
nector_rasterization_api nector_status nectorCreateSurfaceFromPixels(int32_t width, int32_t height, const uint8_t* rgba, size_t pixel_size, nector_surface_handle* out_surface);
nector_rasterization_api void nectorDestroySurface(nector_surface_handle surface);
nector_rasterization_api nector_status nectorSurfaceGetSize(nector_surface_handle surface, int32_t* out_width, int32_t* out_height);
nector_rasterization_api nector_status nectorSurfaceClear(nector_surface_handle surface, nector_color color);
nector_rasterization_api nector_status nectorSurfaceReadPixels(nector_surface_handle surface, uint8_t* out_rgba, size_t pixel_size);
nector_rasterization_api nector_status nectorSurfaceWritePixels(nector_surface_handle surface, const uint8_t* rgba, size_t pixel_size);

nector_rasterization_api nector_status nectorCreatePath(nector_path_handle* out_path);
nector_rasterization_api void nectorDestroyPath(nector_path_handle path);
nector_rasterization_api nector_status nectorPathClear(nector_path_handle path);
nector_rasterization_api nector_status nectorPathMoveTo(nector_path_handle path, float x, float y);
nector_rasterization_api nector_status nectorPathLineTo(nector_path_handle path, float x, float y);
nector_rasterization_api nector_status nectorPathQuadraticTo(nector_path_handle path, float cx, float cy, float x, float y);
nector_rasterization_api nector_status nectorPathCubicTo(nector_path_handle path, float c1x, float c1y, float c2x, float c2y, float x, float y);
nector_rasterization_api nector_status nectorPathClose(nector_path_handle path);
nector_rasterization_api nector_status nectorPathAddRectangle(nector_path_handle path, nector_rect rect);
nector_rasterization_api nector_status nectorPathAddEllipse(nector_path_handle path, float cx, float cy, float rx, float ry);

nector_rasterization_api nector_status nectorCreatePainter(nector_surface_handle surface, nector_painter_handle* out_painter);
nector_rasterization_api void nectorDestroyPainter(nector_painter_handle painter);
nector_rasterization_api nector_status nectorPainterSave(nector_painter_handle painter);
nector_rasterization_api nector_status nectorPainterRestore(nector_painter_handle painter);
nector_rasterization_api nector_status nectorPainterSetClip(nector_painter_handle painter, nector_rect clip);
nector_rasterization_api nector_status nectorPainterResetClip(nector_painter_handle painter);
nector_rasterization_api nector_status nectorPainterSetTransform(nector_painter_handle painter, nector_transform transform);
nector_rasterization_api nector_status nectorPainterSetFillColor(nector_painter_handle painter, nector_color color);
nector_rasterization_api nector_status nectorPainterSetLinearGradient(nector_painter_handle painter, nector_point start, nector_point end, nector_color start_color, nector_color end_color);
nector_rasterization_api nector_status nectorPainterSetRadialGradient(nector_painter_handle painter, nector_point center, float radius, nector_color inner_color, nector_color outer_color);
nector_rasterization_api nector_status nectorPainterFillPath(nector_painter_handle painter, nector_path_handle path, nector_fill_rule fill_rule);
nector_rasterization_api nector_status nectorPainterStrokePath(nector_painter_handle painter, nector_path_handle path, float width, nector_line_cap cap, nector_line_join join);
nector_rasterization_api nector_status nectorPainterStrokePathEx(nector_painter_handle painter, nector_path_handle path, const nector_stroke_style* style);
nector_rasterization_api nector_status nectorPainterFillRectangle(nector_painter_handle painter, nector_rect rect);
nector_rasterization_api nector_status nectorPainterDrawLine(nector_painter_handle painter, nector_point start, nector_point end, float width);
nector_rasterization_api nector_status nectorPainterFillEllipse(nector_painter_handle painter, float cx, float cy, float rx, float ry);
nector_rasterization_api nector_status nectorPainterDrawMask(nector_painter_handle painter, int32_t x, int32_t y, int32_t width, int32_t height, const uint8_t* alpha, size_t alpha_size, int32_t pitch, nector_color color);
nector_rasterization_api nector_status nectorPainterDrawSurface(nector_painter_handle painter, nector_surface_handle source, nector_rect destination, nector_rect source_rect, float opacity);

#ifdef __cplusplus
}
#endif
#endif
