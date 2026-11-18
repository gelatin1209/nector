#ifndef nector_font_cff_h
#define nector_font_cff_h

#include "outline.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace nector_font_detail {

class CffFont {
public:
    CffFont() noexcept;
    ~CffFont();

    CffFont(CffFont&&) noexcept;
    CffFont& operator=(CffFont&&) noexcept;

    CffFont(const CffFont&) = delete;
    CffFont& operator=(const CffFont&) = delete;

    bool parse(const uint8_t* data, size_t size, std::string& error);
    uint16_t glyphCount() const;
    bool glyphOutline(uint16_t glyph_index, OutlineSink& sink, GlyphOutline& out,
                      std::string& error) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
#endif
