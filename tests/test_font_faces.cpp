#include "font.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: test_font_faces <collection>\n";
        return 2;
    }
    try {
        std::ifstream stream(argv[1], std::ios::binary);
        if (!stream) throw std::runtime_error("cannot open collection");
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        int32_t count = 0;
        check(nectorFontGetFaceCount(bytes.data(), bytes.size(), &count));
        if (count < 2) throw std::runtime_error("collection has fewer than two faces");
        for (int32_t index = 0; index < count; ++index) {
            nector_font_handle font = nullptr;
            check(nectorLoadFontMemory(bytes.data(), bytes.size(), index, &font));
            std::unique_ptr<nector_font, decltype(&nectorDestroyFont)> guard(font, nectorDestroyFont);
            nector_text_metrics metrics{};
            check(nectorFontMeasureText(font, 18, "Collection", &metrics));
            if (metrics.width <= 0 || metrics.glyph_count != 10) throw std::runtime_error("face measurement failed");
        }
        if (count > 0) {
            nector_font_handle invalid = nullptr;
            if (nectorLoadFontMemory(bytes.data(), bytes.size(), count, &invalid) != nector_invalid_format || invalid != nullptr) {
                throw std::runtime_error("out-of-range face index was accepted");
            }
        }
        std::cout << "test_font_faces: PASS; " << argv[1] << "; faces=" << count << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_font_faces: FAIL: " << error.what() << '\n';
        return 1;
    }
}
