#include "font.h"
#include "image.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
uint32_t pixel(const std::vector<uint8_t>& pixels, int width, int x, int y) {
    const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
    return (static_cast<uint32_t>(pixels[offset]) << 24) | (static_cast<uint32_t>(pixels[offset + 1]) << 16) |
           (static_cast<uint32_t>(pixels[offset + 2]) << 8) | pixels[offset + 3];
}
}

int main(int argc, char** argv) {
    try {
        const char* font_path = argc > 1 ? argv[1] : "third_party/fonts/deja_vu_sans.ttf";
        nector_surface_handle surface = nullptr;
        check(nectorCreateSurface(160, 120, &surface));
        std::unique_ptr<nector_surface, decltype(&nectorDestroySurface)> surface_guard(surface, nectorDestroySurface);
        check(nectorSurfaceClear(surface, {0, 0, 0, 0}));
        nector_painter_handle painter = nullptr;
        check(nectorCreatePainter(surface, &painter));
        std::unique_ptr<nector_painter, decltype(&nectorDestroyPainter)> painter_guard(painter, nectorDestroyPainter);
        check(nectorPainterSetFillColor(painter, {1, 0, 0, 1}));
        check(nectorPainterFillRectangle(painter, {-10, -10, 40, 40}));
        std::vector<uint8_t> pixels(160 * 120 * 4);
        check(nectorSurfaceReadPixels(surface, pixels.data(), pixels.size()));
        require(pixel(pixels, 160, 0, 0) == 0xff0000ffu, "clipped rectangle did not blend correctly");
        require(pixel(pixels, 160, 40, 40) == 0, "rectangle exceeded its bounds");
        check(nectorPainterSetLinearGradient(painter, {40, 0}, {120, 0}, {0, 0, 1, 1}, {0, 1, 0, 1}));
        check(nectorPainterFillRectangle(painter, {40, 0, 80, 30}));
        nector_path_handle path = nullptr;
        check(nectorCreatePath(&path));
        std::unique_ptr<nector_path, decltype(&nectorDestroyPath)> path_guard(path, nectorDestroyPath);
        check(nectorPathAddEllipse(path, 80, 70, 40, 30));
        check(nectorPainterSetFillColor(painter, {1, 1, 1, 0.5f}));
        check(nectorPainterFillPath(painter, path, nector_fill_nonzero));

        nector_font_handle font = nullptr;
        check(nectorLoadFont(font_path, 0, &font));
        std::unique_ptr<nector_font, decltype(&nectorDestroyFont)> font_guard(font, nectorDestroyFont);
        nector_font_metrics metrics{};
        check(nectorFontGetMetrics(font, 24, &metrics));
        require(metrics.ascent > 0 && metrics.descent < 0 && metrics.line_height > 0, "font metrics are invalid");
        nector_text_metrics text{};
        check(nectorFontMeasureText(font, 24, "AV\nsecond", &text));
        require(text.width > 0 && text.glyph_count == 8 && text.height > metrics.line_height, "text measurement is invalid");
        check(nectorPainterSetTransform(painter, {1, 0, 0, 0, 1, 0}));
        check(nectorFontDrawText(painter, font, 24, 4, 112, "Nector AV", {1, 1, 1, 1}));
        check(nectorFontGetGlyphPath(font, 'A', 32, path));
        int32_t face_count = 0;
        require(nectorFontGetFaceCount(nullptr, 0, &face_count) == nector_invalid_format,
                "empty font was not rejected");
        check(nectorSurfaceReadPixels(surface, pixels.data(), pixels.size()));

        nector_image_handle image = nullptr;
        check(nectorImageCreate(160, 120, pixels.data(), pixels.size(), &image));
        std::unique_ptr<nector_image, decltype(&nectorImageDestroy)> image_guard(image, nectorImageDestroy);
        check(nectorImageSavePng(image, "build/test_graphics.png"));
        check(nectorImageSaveBmp(image, "build/test_graphics.bmp"));
        nector_image_handle loaded = nullptr;
        check(nectorImageLoad("build/test_graphics.png", &loaded));
        std::unique_ptr<nector_image, decltype(&nectorImageDestroy)> loaded_guard(loaded, nectorImageDestroy);
        int32_t width = 0;
        int32_t height = 0;
        check(nectorImageGetSize(loaded, &width, &height));
        require(width == 160 && height == 120, "PNG round-trip dimensions differ");
        loaded_guard.reset();
        loaded = nullptr;
        check(nectorImageLoad("build/test_graphics.bmp", &loaded));
        loaded_guard.reset(loaded);
        check(nectorImageGetSize(loaded, &width, &height));
        require(width == 160 && height == 120, "BMP round-trip dimensions differ");

        const char svg[] = "<svg xmlns='http://www.w3.org/2000/svg' width='32' height='32' viewBox='0 0 32 32'>"
                           "<rect width='32' height='32' fill='#202840'/><circle cx='16' cy='16' r='12' fill='#40c080'/></svg>";
        check(nectorImageLoadSvg(svg, 64, 64, &loaded));
        loaded_guard.reset(loaded);
        check(nectorImageGetSize(loaded, &width, &height));
        require(width == 64 && height == 64, "SVG output size is invalid");
        check(nectorImageSavePng(loaded, "build/test_svg.png"));
        if (argc > 2) {
            loaded_guard.reset();
            loaded = nullptr;
            check(nectorImageLoad(argv[2], &loaded));
            loaded_guard.reset(loaded);
            check(nectorImageGetSize(loaded, &width, &height));
            require(width > 0 && height > 0, "JPEG dimensions are invalid");
        }
        std::cout << "test_graphics: PASS; outputs build/test_graphics.png, build/test_graphics.bmp, build/test_svg.png\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_graphics: FAIL: " << error.what() << '\n';
        return 1;
    }
}
