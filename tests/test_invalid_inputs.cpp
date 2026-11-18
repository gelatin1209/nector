#include "font.h"
#include "image.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void requireRejected(nector_status status, const char* message) {
    require(status != nector_ok, message);
}

} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "usage: test_invalid_inputs <font.ttf>");
        requireRejected(nectorCoreValidateUtf8("\xc0\xaf", 2), "overlong UTF-8 was accepted");
        requireRejected(nectorCoreValidateUtf8("\xed\xa0\x80", 3), "UTF-8 surrogate was accepted");
        requireRejected(nectorCoreValidateUtf8("\xf4\x90\x80\x80", 4), "out-of-range UTF-8 was accepted");

        nector_surface_handle surface = reinterpret_cast<nector_surface_handle>(1);
        requireRejected(nectorCreateSurface(0, 10, &surface), "zero-width surface was accepted");
        require(surface == nullptr, "failed surface creation left a non-null output");
        requireRejected(nectorCreateSurface(-1, 10, &surface), "negative surface size was accepted");
        check(nectorCreateSurface(4, 4, &surface));
        std::unique_ptr<nector_surface, decltype(&nectorDestroySurface)> surface_guard(surface, nectorDestroySurface);
        const std::uint8_t rgba[4 * 4 * 4]{};
        requireRejected(nectorSurfaceWritePixels(surface, rgba, sizeof(rgba) - 1), "short pixel buffer was accepted");
        requireRejected(nectorSurfaceClear(surface, {NAN, 0, 0, 1}), "non-finite color was accepted");
        nector_path_handle path = nullptr;
        check(nectorCreatePath(&path));
        std::unique_ptr<nector_path, decltype(&nectorDestroyPath)> path_guard(path, nectorDestroyPath);
        requireRejected(nectorPathLineTo(path, INFINITY, 0), "non-finite path coordinate was accepted");

        nector_image_handle image = reinterpret_cast<nector_image_handle>(1);
        requireRejected(nectorImageLoadMemory(nullptr, 0, &image), "empty image was accepted");
        require(image == nullptr, "failed image load left a non-null output");
        const std::uint8_t malformed_png[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n', 0, 0, 0};
        requireRejected(nectorImageLoadMemory(malformed_png, sizeof(malformed_png), &image), "truncated PNG was accepted");
        const std::uint8_t malformed_jpeg[] = {0xff, 0xd8, 0xff, 0xe0, 0, 16, 'X'};
        requireRejected(nectorImageLoadMemory(malformed_jpeg, sizeof(malformed_jpeg), &image), "truncated JPEG was accepted");
        const std::uint8_t malformed_bmp[] = {'B', 'M', 20, 0, 0};
        requireRejected(nectorImageLoadMemory(malformed_bmp, sizeof(malformed_bmp), &image), "truncated BMP was accepted");
        requireRejected(nectorImageLoadSvg("<svg><path d='M 0 0 L'></svg>", 20, 20, &image), "malformed SVG path was accepted");
        requireRejected(nectorImageCreate(1000000, 1000000, rgba, sizeof(rgba), &image), "oversized image with short input was accepted");

        nector_font_handle font = reinterpret_cast<nector_font_handle>(1);
        requireRejected(nectorLoadFontMemory(nullptr, 0, 0, &font), "empty font was accepted");
        require(font == nullptr, "failed font load left a non-null output");
        const std::uint8_t malformed_sfnt[] = {0, 1, 0, 0, 0, 1, 0, 0};
        requireRejected(nectorLoadFontMemory(malformed_sfnt, sizeof(malformed_sfnt), 0, &font), "truncated sfnt was accepted");
        const std::uint8_t malformed_woff[] = {'w', 'O', 'F', 'F', 0, 1, 0, 0};
        requireRejected(nectorLoadFontMemory(malformed_woff, sizeof(malformed_woff), 0, &font), "truncated WOFF was accepted");
        const std::uint8_t malformed_woff2[] = {'w', 'O', 'F', '2', 0, 1, 0, 0};
        requireRejected(nectorLoadFontMemory(malformed_woff2, sizeof(malformed_woff2), 0, &font), "truncated WOFF2 was accepted");
        requireRejected(nectorLoadFont(argv[1], -1, &font), "negative face index was accepted");

        check(nectorLoadFont(argv[1], 0, &font));
        std::unique_ptr<nector_font, decltype(&nectorDestroyFont)> font_guard(font, nectorDestroyFont);
        nector_text_metrics text_metrics{};
        requireRejected(nectorFontMeasureText(font, 16, "\x80", &text_metrics), "invalid UTF-8 text was accepted");
        requireRejected(nectorFontMeasureText(font, -1, "A", &text_metrics), "negative font size was accepted");
        nector_font_metrics font_metrics{};
        requireRejected(nectorFontGetMetrics(font, NAN, &font_metrics), "non-finite font size was accepted");

        std::cout << "test_invalid_inputs: PASS; malformed UTF-8, rasterization, PNG, JPEG, BMP, SVG, sfnt, WOFF and WOFF2 rejected\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_invalid_inputs: FAIL: " << error.what() << '\n';
        return 1;
    }
}
