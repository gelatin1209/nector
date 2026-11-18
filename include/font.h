#ifndef nector_font_h
#define nector_font_h

#include "rasterization.h"

#if defined(_WIN32)
  #if defined(nector_font_export)
    #define nector_font_api __declspec(dllexport)
  #else
    #define nector_font_api __declspec(dllimport)
  #endif
#else
  #define nector_font_api __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nector_font nector_font;
typedef nector_font* nector_font_handle;

typedef struct nector_font_metrics {
    float ascent;
    float descent;
    float line_gap;
    float line_height;
} nector_font_metrics;

typedef struct nector_text_metrics {
    float width;
    float height;
    int32_t glyph_count;
} nector_text_metrics;

typedef struct nector_glyph_run_item {
    uint32_t glyph_id;
    uint32_t codepoint;
    size_t cluster;
    float advance;
    float pen_x;
    float baseline_y;
    int32_t line_index;
    nector_rect tight_bounds;
    int32_t has_tight_bounds;
} nector_glyph_run_item;

typedef struct nector_glyph_run {
    const nector_glyph_run_item* items;
    size_t count;
    float width;
    float height;
} nector_glyph_run;

nector_font_api nector_status nectorLoadFont(const char* path, int32_t face_index, nector_font_handle* out_font);
nector_font_api nector_status nectorLoadFontMemory(const uint8_t* data, size_t size, int32_t face_index, nector_font_handle* out_font);
nector_font_api void nectorDestroyFont(nector_font_handle font);
nector_font_api nector_status nectorFontGetFaceCount(const uint8_t* data, size_t size, int32_t* out_count);
nector_font_api nector_status nectorFontGetMetrics(nector_font_handle font, float pixel_size, nector_font_metrics* out_metrics);
nector_font_api nector_status nectorFontMeasureText(nector_font_handle font, float pixel_size, const char* utf8_text, nector_text_metrics* out_metrics);
/* The run and its items are owned by the font and valid until the next layout call on that font. */
nector_font_api nector_status nectorFontLayoutText(nector_font_handle font, float pixel_size, const char* utf8_text, nector_glyph_run* out_run);
nector_font_api nector_status nectorFontDrawText(nector_painter_handle painter, nector_font_handle font, float pixel_size, float x, float baseline_y, const char* utf8_text, nector_color color);
nector_font_api nector_status nectorFontGetGlyphPath(nector_font_handle font, uint32_t codepoint, float pixel_size, nector_path_handle path);

#ifdef __cplusplus
}
#endif
#endif
