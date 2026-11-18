#ifndef nector_rasterization_internal_h
#define nector_rasterization_internal_h

#include "rasterization.h"
#include "../core/internal.h"

#include <cstdint>
#include <vector>

struct nector_path {
    struct Segment {
        enum class Type { line, quadratic, cubic };
        Type type = Type::line;
        nector_point control1{};
        nector_point control2{};
        nector_point end{};
    };
    struct Subpath {
        std::vector<Segment> segments;
        nector_point current{};
        nector_point start{};
        bool closed = false;
    };
    std::vector<Subpath> subpaths;
    size_t command_count = 0;
    bool has_current = false;
};

struct nector_surface {
    int32_t width = 0;
    int32_t height = 0;
    std::vector<uint8_t> pixels;
};

struct nector_painter {
    enum class PaintType { solid, linear, radial };
    struct State {
        nector_color color{0, 0, 0, 1};
        nector_color second_color{0, 0, 0, 1};
        nector_point first_point{};
        nector_point second_point{};
        nector_transform transform{1, 0, 0, 0, 1, 0};
        nector_rect clip{};
        PaintType paint_type = PaintType::solid;
        float radius = 0.0f;
    };
    nector_surface_handle surface = nullptr;
    State state;
    std::vector<State> stack;
};

#endif
