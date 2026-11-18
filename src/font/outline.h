#ifndef nector_font_outline_h
#define nector_font_outline_h

#include <cstdint>

namespace nector_font_detail {

class OutlineSink {
public:
    virtual ~OutlineSink() = default;
    virtual bool moveTo(float x, float y) = 0;
    virtual bool lineTo(float x, float y) = 0;
    virtual bool quadraticTo(float cx, float cy, float x, float y) = 0;
    virtual bool cubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y) = 0;
    virtual bool close() = 0;
};

struct GlyphOutline {
    float advance = 0.0f;
    float left = 0.0f;
    float bottom = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    bool has_bounds = false;
};

}
#endif
