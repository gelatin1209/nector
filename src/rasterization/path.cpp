#include "rasterization_internal.h"

#include <cmath>

namespace {
bool validPoint(float x, float y) {
    return nectorFinite(x) && nectorFinite(y) && std::fabs(x) <= 1.0e7f && std::fabs(y) <= 1.0e7f;
}

nector_status appendSegment(nector_path_handle path, nector_path::Segment::Type type,
                            nector_point control1, nector_point control2, nector_point end) {
    if (!path->has_current || path->subpaths.empty()) {
        return nectorFail(nector_invalid_state, "path has no current subpath");
    }
    auto& subpath = path->subpaths.back();
    if (subpath.closed) {
        return nectorFail(nector_invalid_state, "cannot append to a closed subpath; call moveTo first");
    }
    if (path->command_count >= 1000000) return nectorFail(nector_limit_exceeded, "path command limit exceeded");
    subpath.segments.push_back({type, control1, control2, end});
    ++path->command_count;
    subpath.current = end;
    return nector_ok;
}
}

nector_status nectorCreatePath(nector_path_handle* out_path) {
    if (out_path == nullptr) return nectorFail(nector_invalid_argument, "out_path is null");
    *out_path = nullptr;
    nector_api_try {
        *out_path = new nector_path;
        return nector_ok;
    } nector_api_catch
}

void nectorDestroyPath(nector_path_handle path) {
    delete path;
}

nector_status nectorPathClear(nector_path_handle path) {
    if (path == nullptr) return nectorFail(nector_invalid_argument, "path is null");
    path->subpaths.clear();
    path->command_count = 0;
    path->has_current = false;
    return nector_ok;
}

nector_status nectorPathMoveTo(nector_path_handle path, float x, float y) {
    if (path == nullptr) return nectorFail(nector_invalid_argument, "path is null");
    if (!validPoint(x, y)) return nectorFail(nector_invalid_argument, "path coordinate is invalid");
    nector_api_try {
        if (path->subpaths.size() >= 100000) return nectorFail(nector_limit_exceeded, "subpath limit exceeded");
        nector_path::Subpath subpath;
        subpath.current = {x, y};
        subpath.start = {x, y};
        path->subpaths.push_back(std::move(subpath));
        path->has_current = true;
        return nector_ok;
    } nector_api_catch
}

nector_status nectorPathLineTo(nector_path_handle path, float x, float y) {
    if (path == nullptr) return nectorFail(nector_invalid_argument, "path is null");
    if (!validPoint(x, y)) return nectorFail(nector_invalid_argument, "path coordinate is invalid");
    nector_api_try {
        return appendSegment(path, nector_path::Segment::Type::line, {}, {}, {x, y});
    } nector_api_catch
}

nector_status nectorPathQuadraticTo(nector_path_handle path, float cx, float cy, float x, float y) {
    if (path == nullptr) return nectorFail(nector_invalid_argument, "path is null");
    if (!validPoint(cx, cy) || !validPoint(x, y)) return nectorFail(nector_invalid_argument, "curve coordinate is invalid");
    nector_api_try {
        return appendSegment(path, nector_path::Segment::Type::quadratic, {cx, cy}, {}, {x, y});
    } nector_api_catch
}

nector_status nectorPathCubicTo(nector_path_handle path, float c1x, float c1y, float c2x, float c2y, float x, float y) {
    if (path == nullptr) return nectorFail(nector_invalid_argument, "path is null");
    if (!validPoint(c1x, c1y) || !validPoint(c2x, c2y) || !validPoint(x, y)) {
        return nectorFail(nector_invalid_argument, "curve coordinate is invalid");
    }
    nector_api_try {
        return appendSegment(path, nector_path::Segment::Type::cubic, {c1x, c1y}, {c2x, c2y}, {x, y});
    } nector_api_catch
}

nector_status nectorPathClose(nector_path_handle path) {
    if (path == nullptr) return nectorFail(nector_invalid_argument, "path is null");
    if (!path->has_current || path->subpaths.empty()) return nectorFail(nector_invalid_state, "path has no current subpath");
    auto& subpath = path->subpaths.back();
    if (subpath.closed) return nector_ok;
    if (subpath.segments.empty()) return nectorFail(nector_invalid_state, "cannot close an empty line");
    subpath.closed = true;
    subpath.current = subpath.start;
    return nector_ok;
}

nector_status nectorPathAddRectangle(nector_path_handle path, nector_rect rect) {
    if (path == nullptr) return nectorFail(nector_invalid_argument, "path is null");
    if (!nectorValidRect(rect)) return nectorFail(nector_invalid_argument, "rectangle is invalid");
    nector_status status = nectorPathMoveTo(path, rect.x, rect.y);
    if (status == nector_ok) status = nectorPathLineTo(path, rect.x + rect.width, rect.y);
    if (status == nector_ok) status = nectorPathLineTo(path, rect.x + rect.width, rect.y + rect.height);
    if (status == nector_ok) status = nectorPathLineTo(path, rect.x, rect.y + rect.height);
    if (status == nector_ok) status = nectorPathClose(path);
    return status;
}

nector_status nectorPathAddEllipse(nector_path_handle path, float cx, float cy, float rx, float ry) {
    if (path == nullptr) return nectorFail(nector_invalid_argument, "path is null");
    if (!validPoint(cx, cy) || !nectorFinite(rx) || !nectorFinite(ry) || rx <= 0 || ry <= 0 ||
        rx > 1.0e7f || ry > 1.0e7f) {
        return nectorFail(nector_invalid_argument, "ellipse is invalid");
    }
    constexpr float kappa = 0.5522847498307936f;
    nector_status status = nectorPathMoveTo(path, cx + rx, cy);
    if (status == nector_ok) status = nectorPathCubicTo(path, cx + rx, cy + kappa * ry, cx + kappa * rx, cy + ry, cx, cy + ry);
    if (status == nector_ok) status = nectorPathCubicTo(path, cx - kappa * rx, cy + ry, cx - rx, cy + kappa * ry, cx - rx, cy);
    if (status == nector_ok) status = nectorPathCubicTo(path, cx - rx, cy - kappa * ry, cx - kappa * rx, cy - ry, cx, cy - ry);
    if (status == nector_ok) status = nectorPathCubicTo(path, cx + kappa * rx, cy - ry, cx + rx, cy - kappa * ry, cx + rx, cy);
    if (status == nector_ok) status = nectorPathClose(path);
    return status;
}
