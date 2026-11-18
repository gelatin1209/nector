#include "nector.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

uint8_t alpha(const std::vector<uint8_t>& pixels, int width, int x, int y) {
    return pixels[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4 + 3];
}

void testCoverageAndFillRules() {
    nector::Surface surface(8, 8);
    nector::Painter painter(surface);
    painter.setFillColor({1, 1, 1, 1});
    nector::Path path;
    path.addRectangle({2.25F, 2.5F, 2.0F, 1.0F});
    painter.fillPath(path);
    std::vector<uint8_t> pixels = surface.readPixels();
    require(alpha(pixels, 8, 2, 2) == 96 && alpha(pixels, 8, 3, 2) == 128 &&
            alpha(pixels, 8, 4, 2) == 32, "8-bit horizontal/vertical coverage is incorrect");
    require(alpha(pixels, 8, 2, 3) == 96 && alpha(pixels, 8, 3, 3) == 128 &&
            alpha(pixels, 8, 4, 3) == 32 && alpha(pixels, 8, 3, 1) == 0 &&
            alpha(pixels, 8, 3, 4) == 0, "vertical coverage exceeded the rectangle");

    surface.clear({0, 0, 0, 0});
    path.clear();
    path.addRectangle({1, 1, 6, 6});
    path.addRectangle({2, 2, 2, 2});
    painter.fillPath(path, nector::FillRule::EvenOdd);
    pixels = surface.readPixels();
    require(alpha(pixels, 8, 3, 3) == 0 && alpha(pixels, 8, 1, 1) == 255,
            "even-odd fill did not cut out the inner contour");
    surface.clear({0, 0, 0, 0});
    painter.fillPath(path, nector::FillRule::Nonzero);
    pixels = surface.readPixels();
    require(alpha(pixels, 8, 3, 3) == 255, "nonzero fill discarded a same-winding inner contour");
}

void testCurveDeterminism() {
    nector::Surface surface(64, 64);
    nector::Painter painter(surface);
    painter.setFillColor({1, 1, 1, 1});
    nector::Path path;
    path.moveTo(4, 32);
    path.cubicTo(16, -20, 48, 84, 60, 32);
    path.lineTo(60, 40);
    path.lineTo(4, 40);
    path.close();
    painter.fillPath(path);
    const std::vector<uint8_t> first = surface.readPixels();
    size_t partial = 0;
    size_t covered = 0;
    for (size_t offset = 3; offset < first.size(); offset += 4) {
        if (first[offset] != 0) ++covered;
        if (first[offset] != 0 && first[offset] != 255) ++partial;
    }
    require(covered > 100 && partial > 20, "adaptive cubic fill has insufficient coverage detail");
    surface.clear({0, 0, 0, 0});
    painter.fillPath(path);
    require(surface.readPixels() == first, "repeated cubic rasterization is not deterministic");
}

void horizontalLine(nector::Path& path) {
    path.clear();
    path.moveTo(10, 20);
    path.lineTo(40, 20);
}

void testStrokeCapsAndDashes() {
    nector::Surface surface(64, 64);
    nector::Painter painter(surface);
    painter.setFillColor({1, 1, 1, 1});
    nector::Path path;
    horizontalLine(path);
    nector::StrokeStyle style;
    style.width = 4;
    painter.strokePath(path, style);
    std::vector<uint8_t> pixels = surface.readPixels();
    require(alpha(pixels, 64, 9, 20) == 0 && alpha(pixels, 64, 10, 20) == 255 &&
            alpha(pixels, 64, 39, 20) == 255 && alpha(pixels, 64, 40, 20) == 0,
            "butt caps exceeded the path endpoints");

    surface.clear({0, 0, 0, 0});
    style.cap = nector::LineCap::Square;
    painter.strokePath(path, style);
    pixels = surface.readPixels();
    require(alpha(pixels, 64, 7, 20) == 0 && alpha(pixels, 64, 8, 20) == 255 &&
            alpha(pixels, 64, 41, 20) == 255 && alpha(pixels, 64, 42, 20) == 0,
            "square caps have the wrong extension");

    surface.clear({0, 0, 0, 0});
    style.cap = nector::LineCap::Round;
    painter.strokePath(path, style);
    pixels = surface.readPixels();
    require(alpha(pixels, 64, 9, 20) != 0 && alpha(pixels, 64, 40, 20) != 0 &&
            alpha(pixels, 64, 7, 20) == 0 && alpha(pixels, 64, 42, 20) == 0,
            "round caps are missing or oversized");

    surface.clear({0, 0, 0, 0});
    style.width = 2;
    style.cap = nector::LineCap::Butt;
    style.dashPattern = {6, 4};
    painter.strokePath(path, style);
    pixels = surface.readPixels();
    require(alpha(pixels, 64, 12, 20) == 255 && alpha(pixels, 64, 17, 20) == 0 &&
            alpha(pixels, 64, 22, 20) == 255 && alpha(pixels, 64, 37, 20) == 0,
            "dash pattern produced incorrect on/off intervals");

    surface.clear({0, 0, 0, 0});
    style.dashOffset = 2;
    painter.strokePath(path, style);
    pixels = surface.readPixels();
    require(alpha(pixels, 64, 11, 20) == 255 && alpha(pixels, 64, 15, 20) == 0 &&
            alpha(pixels, 64, 19, 20) == 255, "dash offset did not shift the pattern");

    bool rejected = false;
    try {
        style.dashPattern = {0, 4};
        painter.strokePath(path, style);
    } catch (const nector::Error& error) {
        rejected = error.status() == nector::Status::InvalidArgument;
    }
    require(rejected, "zero-length dash was accepted");
}

void testMiterLimit() {
    nector::Surface surface(64, 64);
    nector::Painter painter(surface);
    painter.setFillColor({1, 1, 1, 1});
    nector::Path path;
    path.moveTo(10, 30);
    path.lineTo(30, 10);
    path.lineTo(50, 30);
    nector::StrokeStyle style;
    style.width = 8;
    style.miterLimit = 10;
    painter.strokePath(path, style);
    require(alpha(surface.readPixels(), 64, 30, 5) != 0, "acute miter tip was not rendered");
    surface.clear({0, 0, 0, 0});
    style.miterLimit = 1;
    painter.strokePath(path, style);
    require(alpha(surface.readPixels(), 64, 30, 5) == 0, "miter limit did not fall back to a bevel");
}

}

int main() {
    try {
        testCoverageAndFillRules();
        testCurveDeterminism();
        testStrokeCapsAndDashes();
        testMiterLimit();
        std::cout << "test_rasterization: PASS; 8-bit coverage, fill rules, curves, caps, joins, miter limit and dashes\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_rasterization: FAIL: " << error.what() << '\n';
        return 1;
    }
}
