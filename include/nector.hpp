#ifndef nector_nector_hpp
#define nector_nector_hpp

#include <core.h>
#include <rasterization.h>
#include <font.h>
#include <image.h>
#include <nimation.h>
#include <input.h>
#include <scene.h>
#include <audio.h>
#include <platform.h>
#include <dsl_comp.h>
#include <interface.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace nector {

enum class Status : int {
    Ok = nector_ok,
    InvalidArgument = nector_invalid_argument,
    OutOfMemory = nector_out_of_memory,
    IoError = nector_io_error,
    InvalidFormat = nector_invalid_format,
    Unsupported = nector_unsupported,
    InvalidState = nector_invalid_state,
    NotFound = nector_not_found,
    LimitExceeded = nector_limit_exceeded,
    BackendError = nector_backend_error,
    CallbackError = nector_callback_error
};

class Error : public std::runtime_error {
public:
    Error(Status status, const std::string& message)
        : std::runtime_error(message), status_(status) {}

    Status status() const noexcept { return status_; }

private:
    Status status_;
};

namespace detail {

inline std::string errorMessage(nector_status status) {
    const char* status_text = nectorStatusText(status);
    std::string message = status_text != nullptr ? status_text : "Nector error";
    const char* last_error = nectorGetLastError();
    if (last_error != nullptr && *last_error != '\0') {
        message += ": ";
        message += last_error;
    }
    return message;
}

[[noreturn]] inline void throwError(nector_status status) {
    throw Error(static_cast<Status>(status), errorMessage(status));
}

inline void check(nector_status status) {
    if (status != nector_ok) {
        throwError(status);
    }
}

template <typename HandleType, void (*Destroy)(HandleType)>
class ResourceHandle {
public:
    ResourceHandle(const ResourceHandle&) = delete;
    ResourceHandle& operator=(const ResourceHandle&) = delete;

    ResourceHandle(ResourceHandle&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)),
          owns_(std::exchange(other.owns_, true)) {}

    ResourceHandle& operator=(ResourceHandle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
            owns_ = std::exchange(other.owns_, true);
        }
        return *this;
    }

    HandleType nativeHandle() const noexcept { return handle_; }
    explicit operator bool() const noexcept { return handle_ != nullptr; }

protected:
    ResourceHandle() noexcept = default;

    explicit ResourceHandle(HandleType handle, bool owns = true) noexcept
        : handle_(handle), owns_(owns) {}

    ~ResourceHandle() { reset(); }

    void reset() noexcept {
        if (handle_ != nullptr && owns_) {
            Destroy(handle_);
        }
        handle_ = nullptr;
        owns_ = true;
    }

    HandleType handle_ = nullptr;
    bool owns_ = true;
};

} // namespace detail

inline std::string getVersion() {
    const char* value = nectorGetVersion();
    return value != nullptr ? std::string(value) : std::string();
}

inline std::string getLastError() {
    const char* value = nectorGetLastError();
    return value != nullptr ? std::string(value) : std::string();
}

inline void clearError() noexcept { nectorClearError(); }

inline std::string statusText(Status status) {
    const char* value = nectorStatusText(static_cast<nector_status>(status));
    return value != nullptr ? std::string(value) : std::string();
}

inline void setCoreError(Status status, const std::string& message) noexcept {
    nectorCoreSetError(static_cast<nector_status>(status), message.c_str());
}

inline void validateUtf8(const char* text, std::size_t length) {
    detail::check(nectorCoreValidateUtf8(text, length));
}

inline void validateUtf8(const std::string& text) {
    validateUtf8(text.data(), text.size());
}

inline std::uint32_t crc32(const std::uint8_t* data, std::size_t length) noexcept {
    return nectorCoreCrc32(data, length);
}

inline std::uint32_t crc32(const std::vector<std::uint8_t>& data) noexcept {
    return crc32(data.data(), data.size());
}

using Point = nector_point;
using Rect = nector_rect;
using Color = nector_color;
using Transform = nector_transform;
using FontMetrics = nector_font_metrics;
using TextMetrics = nector_text_metrics;
using GlyphRunItem = nector_glyph_run_item;

struct GlyphRun {
    std::vector<GlyphRunItem> items;
    float width = 0.0F;
    float height = 0.0F;
};

struct Size {
    std::int32_t width = 0;
    std::int32_t height = 0;
};

namespace detail {

inline std::size_t rgbaSize(const Size& size) {
    return static_cast<std::size_t>(size.width) *
           static_cast<std::size_t>(size.height) * 4U;
}

} // namespace detail

enum class FillRule : int {
    Nonzero = nector_fill_nonzero,
    EvenOdd = nector_fill_even_odd
};

enum class LineCap : int {
    Butt = nector_cap_butt,
    Round = nector_cap_round,
    Square = nector_cap_square
};

enum class LineJoin : int {
    Miter = nector_join_miter,
    Round = nector_join_round,
    Bevel = nector_join_bevel
};

struct StrokeStyle {
    float width = 1.0F;
    LineCap cap = LineCap::Butt;
    LineJoin join = LineJoin::Miter;
    float miterLimit = 4.0F;
    float dashOffset = 0.0F;
    std::vector<float> dashPattern;
};

class Surface : public detail::ResourceHandle<nector_surface_handle, nectorDestroySurface> {
    using Base = detail::ResourceHandle<nector_surface_handle, nectorDestroySurface>;

public:
    Surface() noexcept = default;

    Surface(std::int32_t width, std::int32_t height) {
        detail::check(nectorCreateSurface(width, height, &handle_));
    }

    Surface(Surface&&) noexcept = default;
    Surface& operator=(Surface&&) noexcept = default;

    static Surface fromPixels(std::int32_t width, std::int32_t height,
                              const std::uint8_t* rgba, std::size_t pixel_size) {
        nector_surface_handle handle = nullptr;
        detail::check(nectorCreateSurfaceFromPixels(width, height, rgba, pixel_size, &handle));
        return Surface(handle);
    }

    static Surface fromPixels(std::int32_t width, std::int32_t height,
                              const std::vector<std::uint8_t>& rgba) {
        return fromPixels(width, height, rgba.data(), rgba.size());
    }

    Size size() const {
        Size result;
        detail::check(nectorSurfaceGetSize(handle_, &result.width, &result.height));
        return result;
    }

    void clear(Color color) const {
        detail::check(nectorSurfaceClear(handle_, color));
    }

    void readPixels(std::uint8_t* out_rgba, std::size_t pixel_size) const {
        detail::check(nectorSurfaceReadPixels(handle_, out_rgba, pixel_size));
    }

    std::vector<std::uint8_t> readPixels() const {
        std::vector<std::uint8_t> pixels(detail::rgbaSize(size()));
        readPixels(pixels.data(), pixels.size());
        return pixels;
    }

    void writePixels(const std::uint8_t* rgba, std::size_t pixel_size) const {
        detail::check(nectorSurfaceWritePixels(handle_, rgba, pixel_size));
    }

    void writePixels(const std::vector<std::uint8_t>& rgba) const {
        writePixels(rgba.data(), rgba.size());
    }

private:
    explicit Surface(nector_surface_handle handle) : Base(handle) {}
};

class Path : public detail::ResourceHandle<nector_path_handle, nectorDestroyPath> {
    using Base = detail::ResourceHandle<nector_path_handle, nectorDestroyPath>;

public:
    Path() { detail::check(nectorCreatePath(&handle_)); }
    Path(Path&&) noexcept = default;
    Path& operator=(Path&&) noexcept = default;

    void clear() const { detail::check(nectorPathClear(handle_)); }

    void moveTo(float x, float y) const {
        detail::check(nectorPathMoveTo(handle_, x, y));
    }

    void lineTo(float x, float y) const {
        detail::check(nectorPathLineTo(handle_, x, y));
    }

    void quadraticTo(float cx, float cy, float x, float y) const {
        detail::check(nectorPathQuadraticTo(handle_, cx, cy, x, y));
    }

    void cubicTo(float c1x, float c1y, float c2x, float c2y,
                 float x, float y) const {
        detail::check(nectorPathCubicTo(handle_, c1x, c1y, c2x, c2y, x, y));
    }

    void close() const { detail::check(nectorPathClose(handle_)); }

    void addRectangle(Rect rect) const {
        detail::check(nectorPathAddRectangle(handle_, rect));
    }

    void addEllipse(float cx, float cy, float rx, float ry) const {
        detail::check(nectorPathAddEllipse(handle_, cx, cy, rx, ry));
    }
};

class Painter : public detail::ResourceHandle<nector_painter_handle, nectorDestroyPainter> {
    using Base = detail::ResourceHandle<nector_painter_handle, nectorDestroyPainter>;

public:
    Painter() noexcept = default;

    explicit Painter(const Surface& surface) {
        detail::check(nectorCreatePainter(surface.nativeHandle(), &handle_));
    }

    Painter(Painter&&) noexcept = default;
    Painter& operator=(Painter&&) noexcept = default;

    void save() const { detail::check(nectorPainterSave(handle_)); }
    void restore() const { detail::check(nectorPainterRestore(handle_)); }

    void setClip(Rect clip) const {
        detail::check(nectorPainterSetClip(handle_, clip));
    }

    void resetClip() const { detail::check(nectorPainterResetClip(handle_)); }

    void setTransform(Transform transform) const {
        detail::check(nectorPainterSetTransform(handle_, transform));
    }

    void setFillColor(Color color) const {
        detail::check(nectorPainterSetFillColor(handle_, color));
    }

    void setLinearGradient(Point start, Point end, Color start_color,
                           Color end_color) const {
        detail::check(nectorPainterSetLinearGradient(handle_, start, end,
                                                     start_color, end_color));
    }

    void setRadialGradient(Point center, float radius, Color inner_color,
                           Color outer_color) const {
        detail::check(nectorPainterSetRadialGradient(handle_, center, radius,
                                                     inner_color, outer_color));
    }

    void fillPath(const Path& path, FillRule fill_rule = FillRule::Nonzero) const {
        detail::check(nectorPainterFillPath(handle_, path.nativeHandle(),
                                            static_cast<nector_fill_rule>(fill_rule)));
    }

    void strokePath(const Path& path, float width,
                    LineCap cap = LineCap::Butt,
                    LineJoin join = LineJoin::Miter) const {
        detail::check(nectorPainterStrokePath(handle_, path.nativeHandle(), width,
                                              static_cast<nector_line_cap>(cap),
                                              static_cast<nector_line_join>(join)));
    }

    void strokePath(const Path& path, const StrokeStyle& style) const {
        const nector_stroke_style native{
            style.width, static_cast<nector_line_cap>(style.cap),
            static_cast<nector_line_join>(style.join), style.miterLimit,
            style.dashOffset, style.dashPattern.data(), style.dashPattern.size()};
        detail::check(nectorPainterStrokePathEx(handle_, path.nativeHandle(), &native));
    }

    void fillRectangle(Rect rect) const {
        detail::check(nectorPainterFillRectangle(handle_, rect));
    }

    void drawLine(Point start, Point end, float width) const {
        detail::check(nectorPainterDrawLine(handle_, start, end, width));
    }

    void fillEllipse(float cx, float cy, float rx, float ry) const {
        detail::check(nectorPainterFillEllipse(handle_, cx, cy, rx, ry));
    }

    void drawMask(std::int32_t x, std::int32_t y, std::int32_t width,
                  std::int32_t height, const std::uint8_t* alpha,
                  std::size_t alpha_size, std::int32_t pitch, Color color) const {
        detail::check(nectorPainterDrawMask(handle_, x, y, width, height, alpha,
                                            alpha_size, pitch, color));
    }

    void drawMask(std::int32_t x, std::int32_t y, std::int32_t width,
                  std::int32_t height, const std::vector<std::uint8_t>& alpha,
                  std::int32_t pitch, Color color) const {
        drawMask(x, y, width, height, alpha.data(), alpha.size(), pitch, color);
    }

    void drawSurface(const Surface& source, Rect destination, Rect source_rect,
                     float opacity = 1.0F) const {
        detail::check(nectorPainterDrawSurface(handle_, source.nativeHandle(),
                                               destination, source_rect, opacity));
    }

private:
    explicit Painter(nector_painter_handle handle, bool owns)
        : Base(handle, owns) {}

    static Painter borrow(nector_painter_handle handle) {
        return Painter(handle, false);
    }

    friend struct UiEvent;
};

class Font : public detail::ResourceHandle<nector_font_handle, nectorDestroyFont> {
    using Base = detail::ResourceHandle<nector_font_handle, nectorDestroyFont>;

public:
    explicit Font(const std::string& path, std::int32_t face_index = 0) {
        detail::check(nectorLoadFont(path.c_str(), face_index, &handle_));
    }

    Font(Font&&) noexcept = default;
    Font& operator=(Font&&) noexcept = default;

    static Font fromMemory(const std::uint8_t* data, std::size_t size,
                           std::int32_t face_index = 0) {
        nector_font_handle handle = nullptr;
        detail::check(nectorLoadFontMemory(data, size, face_index, &handle));
        return Font(handle);
    }

    static Font fromMemory(const std::vector<std::uint8_t>& data,
                           std::int32_t face_index = 0) {
        return fromMemory(data.data(), data.size(), face_index);
    }

    static std::int32_t faceCount(const std::uint8_t* data, std::size_t size) {
        std::int32_t count = 0;
        detail::check(nectorFontGetFaceCount(data, size, &count));
        return count;
    }

    static std::int32_t faceCount(const std::vector<std::uint8_t>& data) {
        return faceCount(data.data(), data.size());
    }

    FontMetrics metrics(float pixel_size) const {
        FontMetrics result{};
        detail::check(nectorFontGetMetrics(handle_, pixel_size, &result));
        return result;
    }

    TextMetrics measureText(float pixel_size, const std::string& utf8_text) const {
        TextMetrics result{};
        detail::check(nectorFontMeasureText(handle_, pixel_size, utf8_text.c_str(), &result));
        return result;
    }

    GlyphRun layoutText(float pixel_size, const std::string& utf8_text) const {
        nector_glyph_run native{};
        detail::check(nectorFontLayoutText(handle_, pixel_size, utf8_text.c_str(), &native));
        GlyphRun result;
        if (native.items != nullptr && native.count != 0) {
            result.items.assign(native.items, native.items + native.count);
        }
        result.width = native.width;
        result.height = native.height;
        return result;
    }

    void drawText(const Painter& painter, float pixel_size, float x,
                  float baseline_y, const std::string& utf8_text, Color color) const {
        detail::check(nectorFontDrawText(painter.nativeHandle(), handle_, pixel_size,
                                         x, baseline_y, utf8_text.c_str(), color));
    }

    void getGlyphPath(std::uint32_t codepoint, float pixel_size, Path& path) const {
        detail::check(nectorFontGetGlyphPath(handle_, codepoint, pixel_size,
                                             path.nativeHandle()));
    }

private:
    explicit Font(nector_font_handle handle) : Base(handle) {}
};

class Image;

namespace detail {
Image imageFromRetainedHandle(nector_image_handle handle);
}

enum class ImageFormat : int {
    Unknown = nector_image_unknown,
    Png = nector_image_png,
    Jpeg = nector_image_jpeg,
    Bmp = nector_image_bmp,
    Svg = nector_image_svg
};

class Image : public detail::ResourceHandle<nector_image_handle, nectorImageDestroy> {
    using Base = detail::ResourceHandle<nector_image_handle, nectorImageDestroy>;

public:
    Image() noexcept = default;

    explicit Image(const std::string& path) {
        detail::check(nectorImageLoad(path.c_str(), &handle_));
    }

    Image(Image&&) noexcept = default;
    Image& operator=(Image&&) noexcept = default;

    static Image load(const std::string& path) { return Image(path); }

    static Image loadMemory(const std::uint8_t* data, std::size_t size) {
        nector_image_handle handle = nullptr;
        detail::check(nectorImageLoadMemory(data, size, &handle));
        return Image(handle);
    }

    static Image loadMemory(const std::vector<std::uint8_t>& data) {
        return loadMemory(data.data(), data.size());
    }

    static Image loadSvg(const std::string& utf8_svg, std::int32_t output_width,
                         std::int32_t output_height) {
        nector_image_handle handle = nullptr;
        detail::check(nectorImageLoadSvg(utf8_svg.c_str(), output_width,
                                         output_height, &handle));
        return Image(handle);
    }

    static Image create(std::int32_t width, std::int32_t height,
                        const std::uint8_t* rgba, std::size_t pixel_size) {
        nector_image_handle handle = nullptr;
        detail::check(nectorImageCreate(width, height, rgba, pixel_size, &handle));
        return Image(handle);
    }

    static Image create(std::int32_t width, std::int32_t height,
                        const std::vector<std::uint8_t>& rgba) {
        return create(width, height, rgba.data(), rgba.size());
    }

    Size size() const {
        Size result;
        detail::check(nectorImageGetSize(handle_, &result.width, &result.height));
        return result;
    }

    void readPixels(std::uint8_t* out_rgba, std::size_t pixel_size) const {
        detail::check(nectorImageReadPixels(handle_, out_rgba, pixel_size));
    }

    std::vector<std::uint8_t> readPixels() const {
        std::vector<std::uint8_t> pixels(detail::rgbaSize(size()));
        readPixels(pixels.data(), pixels.size());
        return pixels;
    }

    void draw(const Painter& painter, Rect destination, Rect source_rect,
              float opacity = 1.0F) const {
        detail::check(nectorImageDraw(painter.nativeHandle(), handle_, destination,
                                      source_rect, opacity));
    }

    void savePng(const std::string& path) const {
        detail::check(nectorImageSavePng(handle_, path.c_str()));
    }

    void saveBmp(const std::string& path) const {
        detail::check(nectorImageSaveBmp(handle_, path.c_str()));
    }

private:
    explicit Image(nector_image_handle handle) : Base(handle) {}

    friend Image detail::imageFromRetainedHandle(nector_image_handle handle);
};

namespace detail {
inline Image imageFromRetainedHandle(nector_image_handle handle) {
    return Image(handle);
}
}

enum class Easing : int {
    Linear = nector_ease_linear,
    In = nector_ease_in,
    Out = nector_ease_out,
    InOut = nector_ease_in_out,
    Smooth = nector_ease_smooth,
    Bounce = nector_ease_bounce,
    CubicBezier = nector_ease_cubic_bezier,
    Steps = nector_ease_steps
};

enum class StepPosition : int {
    End = nector_step_end,
    Start = nector_step_start
};

enum class PlaybackDirection : int {
    Normal = nector_playback_normal,
    Reverse = nector_playback_reverse,
    PingPong = nector_playback_ping_pong
};

enum class AnimationEventType : int {
    Frame = nector_animation_frame_event,
    Loop = nector_animation_loop_event,
    Finish = nector_animation_finish_event
};

using EasingParameters = nector_easing_parameters;
using AnimationEvent = nector_animation_event;
using AnimationEventCallback = std::function<void(const AnimationEvent&)>;

namespace detail {

struct AnimationCallbackState {
    AnimationEventCallback callback;
};

inline void dispatchAnimationEvent(const nector_animation_event* event, void* user_data) {
    AnimationCallbackState& state = *static_cast<AnimationCallbackState*>(user_data);
    if (state.callback && event != nullptr) state.callback(*event);
}

}

using NImage = Image;

class Animation : public detail::ResourceHandle<nector_animation_handle, nectorDestroyAnimation> {
    using Base = detail::ResourceHandle<nector_animation_handle, nectorDestroyAnimation>;

public:
    explicit Animation(double duration_ms) {
        detail::check(nectorCreateAnimation(duration_ms, &handle_));
    }

    Animation(Animation&&) noexcept = default;
    Animation& operator=(Animation&&) noexcept = default;

    void addKey(std::int32_t channel, double time_ms, double value,
                Easing easing = Easing::Linear) const {
        detail::check(nectorAnimationAddKey(handle_, channel, time_ms, value,
                                            static_cast<nector_easing>(easing)));
    }

    void addKey(std::int32_t channel, double time_ms, double value,
                Easing easing, const EasingParameters& parameters) const {
        detail::check(nectorAnimationAddKeyEx(handle_, channel, time_ms, value,
                                              static_cast<nector_easing>(easing), &parameters));
    }

    void setLoop(std::int32_t loop_count) const {
        detail::check(nectorAnimationSetLoop(handle_, loop_count));
    }

    void start() const { detail::check(nectorAnimationStart(handle_)); }
    void pause() const { detail::check(nectorAnimationPause(handle_)); }
    void stop() const { detail::check(nectorAnimationStop(handle_)); }

    void seek(double time_ms) const {
        detail::check(nectorAnimationSeek(handle_, time_ms));
    }

    void tick(double delta_ms) const {
        detail::check(nectorAnimationTick(handle_, delta_ms));
    }

    double sample(std::int32_t channel) const {
        double value = 0.0;
        detail::check(nectorAnimationSample(handle_, channel, &value));
        return value;
    }

    double position() const {
        double value = 0.0;
        detail::check(nectorAnimationGetPosition(handle_, &value));
        return value;
    }

    bool isPlaying() const {
        std::int32_t value = 0;
        detail::check(nectorAnimationIsPlaying(handle_, &value));
        return value != 0;
    }

    bool isFinished() const {
        std::int32_t value = 0;
        detail::check(nectorAnimationIsFinished(handle_, &value));
        return value != 0;
    }

    void setDirection(PlaybackDirection direction) const {
        detail::check(nectorAnimationSetDirection(handle_, static_cast<nector_playback_direction>(direction)));
    }

    PlaybackDirection direction() const {
        nector_playback_direction value = nector_playback_normal;
        detail::check(nectorAnimationGetDirection(handle_, &value));
        return static_cast<PlaybackDirection>(value);
    }

    void setRate(double rate) const { detail::check(nectorAnimationSetRate(handle_, rate)); }

    double rate() const {
        double value = 1.0;
        detail::check(nectorAnimationGetRate(handle_, &value));
        return value;
    }

    std::uint64_t completedLoops() const {
        std::uint64_t value = 0;
        detail::check(nectorAnimationGetCompletedLoops(handle_, &value));
        return value;
    }

    void setEventCallback(AnimationEventCallback callback) const {
        callback_state_->callback = std::move(callback);
        detail::check(nectorAnimationSetEventCallback(
            handle_, callback_state_->callback ? &detail::dispatchAnimationEvent : nullptr,
            callback_state_->callback ? callback_state_.get() : nullptr));
    }

private:
    std::shared_ptr<detail::AnimationCallbackState> callback_state_ =
        std::make_shared<detail::AnimationCallbackState>();
};

namespace detail {
class FrameAnimation;
}

class Timeline {
public:
    void setLoop(std::int32_t loop_count) const {
        detail::check(nectorAnimationSetLoop(handle_, loop_count));
    }

    void start() const { detail::check(nectorAnimationStart(handle_)); }
    void pause() const { detail::check(nectorAnimationPause(handle_)); }
    void stop() const { detail::check(nectorAnimationStop(handle_)); }

    void seek(double time_ms) const {
        detail::check(nectorAnimationSeek(handle_, time_ms));
    }

    void seekFrame(std::int32_t frame_index) const {
        detail::check(nectorAnimationSeekFrame(handle_, frame_index));
    }

    void tick(double delta_ms) const {
        detail::check(nectorAnimationTick(handle_, delta_ms));
    }

    std::int32_t frameCount() const {
        std::int32_t value = 0;
        detail::check(nectorAnimationGetFrameCount(handle_, &value));
        return value;
    }

    double frameDuration() const {
        double value = 0.0;
        detail::check(nectorAnimationGetFrameDuration(handle_, &value));
        return value;
    }

    std::int32_t currentFrame() const {
        std::int32_t value = 0;
        detail::check(nectorAnimationGetCurrentFrame(handle_, &value));
        return value;
    }

    double position() const {
        double value = 0.0;
        detail::check(nectorAnimationGetPosition(handle_, &value));
        return value;
    }

    bool isPlaying() const {
        std::int32_t value = 0;
        detail::check(nectorAnimationIsPlaying(handle_, &value));
        return value != 0;
    }

    bool isFinished() const {
        std::int32_t value = 0;
        detail::check(nectorAnimationIsFinished(handle_, &value));
        return value != 0;
    }

    void setDirection(PlaybackDirection direction) const {
        detail::check(nectorAnimationSetDirection(handle_, static_cast<nector_playback_direction>(direction)));
    }

    PlaybackDirection direction() const {
        nector_playback_direction value = nector_playback_normal;
        detail::check(nectorAnimationGetDirection(handle_, &value));
        return static_cast<PlaybackDirection>(value);
    }

    void setRate(double rate) const { detail::check(nectorAnimationSetRate(handle_, rate)); }

    double rate() const {
        double value = 1.0;
        detail::check(nectorAnimationGetRate(handle_, &value));
        return value;
    }

    std::uint64_t completedLoops() const {
        std::uint64_t value = 0;
        detail::check(nectorAnimationGetCompletedLoops(handle_, &value));
        return value;
    }

    void setEventCallback(AnimationEventCallback callback) const {
        callback_state_->callback = std::move(callback);
        detail::check(nectorAnimationSetEventCallback(
            handle_, callback_state_->callback ? &detail::dispatchAnimationEvent : nullptr,
            callback_state_->callback ? callback_state_.get() : nullptr));
    }

private:
    explicit Timeline(nector_animation_handle handle,
                      std::shared_ptr<detail::AnimationCallbackState> callback_state) noexcept
        : handle_(handle), callback_state_(std::move(callback_state)) {}

    nector_animation_handle handle_;
    std::shared_ptr<detail::AnimationCallbackState> callback_state_;

    friend class detail::FrameAnimation;
};

namespace detail {

class FrameAnimation : public ResourceHandle<nector_animation_handle, nectorDestroyAnimation> {
    using Base = ResourceHandle<nector_animation_handle, nectorDestroyAnimation>;

public:
    FrameAnimation(FrameAnimation&&) noexcept = default;
    FrameAnimation& operator=(FrameAnimation&&) noexcept = default;

    Timeline timeline() const { return Timeline(handle_, callback_state_); }

    void addKey(std::int32_t channel, double time_ms, double value,
                Easing easing = Easing::Linear) const {
        detail::check(nectorAnimationAddKey(handle_, channel, time_ms, value,
                                            static_cast<nector_easing>(easing)));
    }

    void addKey(std::int32_t channel, double time_ms, double value,
                Easing easing, const EasingParameters& parameters) const {
        detail::check(nectorAnimationAddKeyEx(handle_, channel, time_ms, value,
                                              static_cast<nector_easing>(easing), &parameters));
    }

    void setLoop(std::int32_t loop_count) const { timeline().setLoop(loop_count); }
    void start() const { timeline().start(); }
    void pause() const { timeline().pause(); }
    void stop() const { timeline().stop(); }
    void seek(double time_ms) const { timeline().seek(time_ms); }
    void seekFrame(std::int32_t frame_index) const { timeline().seekFrame(frame_index); }
    void tick(double delta_ms) const { timeline().tick(delta_ms); }

    double sample(std::int32_t channel) const {
        double value = 0.0;
        detail::check(nectorAnimationSample(handle_, channel, &value));
        return value;
    }

    double position() const { return timeline().position(); }
    std::int32_t currentFrame() const { return timeline().currentFrame(); }
    bool isPlaying() const { return timeline().isPlaying(); }
    bool isFinished() const { return timeline().isFinished(); }
    void setDirection(PlaybackDirection direction) const { timeline().setDirection(direction); }
    PlaybackDirection direction() const { return timeline().direction(); }
    void setRate(double rate) const { timeline().setRate(rate); }
    double rate() const { return timeline().rate(); }
    std::uint64_t completedLoops() const { return timeline().completedLoops(); }
    void setEventCallback(AnimationEventCallback callback) const {
        timeline().setEventCallback(std::move(callback));
    }

protected:
    FrameAnimation() = default;

    std::shared_ptr<AnimationCallbackState> callback_state_ =
        std::make_shared<AnimationCallbackState>();
};

} // namespace detail

class NImation : public detail::FrameAnimation {
public:
    using Timeline = nector::Timeline;

    enum class Behaviour : int {
        Forward = nector_behaviour_forward,
        Backward = nector_behaviour_backward,
        TurnLeft = nector_behaviour_turn_left,
        TurnRight = nector_behaviour_turn_right,
        Hide = nector_behaviour_hide,
        Scale = nector_behaviour_scale,
        Fade = nector_behaviour_fade
    };

    struct Transform {
        double x = 0.0;
        double y = 0.0;
        double rotationDegrees = 0.0;
        bool visible = true;
        double scale = 1.0;
        double opacity = 1.0;
    };

    explicit NImation(std::int32_t frame_count, double frame_duration_ms = 1000.0 / 60.0) {
        detail::check(nectorCreateFrameAnimation(frame_count, frame_duration_ms, &handle_));
    }

    NImation(NImation&&) noexcept = default;
    NImation& operator=(NImation&&) noexcept = default;

    void addBehaviour(Behaviour behaviour, std::int32_t start_frame, std::int32_t end_frame,
                      double start_value, double end_value) const {
        detail::check(nectorAnimationAddBehaviour(handle_, static_cast<nector_behaviour>(behaviour),
                                                  start_frame, end_frame, start_value, end_value));
    }

    Transform sampleTransform() const {
        nector_behaviour_sample_ex sample{};
        detail::check(nectorAnimationSampleBehaviourEx(handle_, &sample));
        return {sample.x, sample.y, sample.rotation_degrees, sample.visible != 0,
                sample.scale, sample.opacity};
    }

    class Flipbook : public detail::FrameAnimation {
    public:
        explicit Flipbook(std::int32_t frame_count, double frame_duration_ms = 1000.0 / 60.0) {
            detail::check(nectorCreateFlipbook(frame_count, frame_duration_ms, &handle_));
        }

        Flipbook(Flipbook&&) noexcept = default;
        Flipbook& operator=(Flipbook&&) noexcept = default;

        void setFrame(std::int32_t frame_index, const Image& image) const {
            detail::check(nectorFlipbookSetFrame(handle_, frame_index, image.nativeHandle()));
        }

        Image currentImage() const {
            nector_image_handle image = nullptr;
            detail::check(nectorFlipbookGetCurrentImage(handle_, &image));
            return detail::imageFromRetainedHandle(image);
        }
    };

    class Hybrid : public detail::FrameAnimation {
    public:
        explicit Hybrid(std::int32_t frame_count, double frame_duration_ms = 1000.0 / 60.0) {
            detail::check(nectorCreateHybrid(frame_count, frame_duration_ms, &handle_));
        }

        Hybrid(Hybrid&&) noexcept = default;
        Hybrid& operator=(Hybrid&&) noexcept = default;

        void addBehaviour(Behaviour behaviour, std::int32_t start_frame, std::int32_t end_frame,
                          double start_value, double end_value) const {
            detail::check(nectorAnimationAddBehaviour(handle_, static_cast<nector_behaviour>(behaviour),
                                                      start_frame, end_frame, start_value, end_value));
        }

        Transform sampleTransform() const {
            nector_behaviour_sample_ex sample{};
            detail::check(nectorAnimationSampleBehaviourEx(handle_, &sample));
            return {sample.x, sample.y, sample.rotation_degrees, sample.visible != 0,
                    sample.scale, sample.opacity};
        }

        void setFrame(std::int32_t frame_index, const Image& image) const {
            detail::check(nectorFlipbookSetFrame(handle_, frame_index, image.nativeHandle()));
        }

        Image currentImage() const {
            nector_image_handle image = nullptr;
            detail::check(nectorFlipbookGetCurrentImage(handle_, &image));
            return detail::imageFromRetainedHandle(image);
        }
    };
};

enum class InputEventType : int {
    None = nector_event_none,
    KeyDown = nector_event_key_down,
    KeyUp = nector_event_key_up,
    TextInput = nector_event_text_input,
    MouseMove = nector_event_mouse_move,
    MouseDown = nector_event_mouse_down,
    MouseUp = nector_event_mouse_up,
    MouseWheel = nector_event_mouse_wheel,
    TouchBegin = nector_event_touch_begin,
    TouchMove = nector_event_touch_move,
    TouchEnd = nector_event_touch_end,
    TouchCancel = nector_event_touch_cancel,
    FocusLost = nector_event_focus_lost,
    WindowClose = nector_event_window_close,
    WindowResize = nector_event_window_resize
};

enum class Key : int {
    Unknown = nector_key_unknown,
    Backspace = nector_key_backspace,
    Tab = nector_key_tab,
    Enter = nector_key_enter,
    Escape = nector_key_escape,
    Space = nector_key_space,
    Left = nector_key_left,
    Right = nector_key_right,
    Up = nector_key_up,
    Down = nector_key_down,
    Home = nector_key_home,
    End = nector_key_end,
    Delete = nector_key_delete,
    Shift = nector_key_shift,
    Control = nector_key_control,
    Alt = nector_key_alt
};

struct InputEvent {
    InputEventType type = InputEventType::None;
    std::int32_t key = 0;
    std::int32_t button = 0;
    std::uint32_t modifiers = 0;
    std::int64_t touch_id = 0;
    float x = 0.0F;
    float y = 0.0F;
    float dx = 0.0F;
    float dy = 0.0F;
    float pressure = 0.0F;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::string text;

    InputEvent() = default;

    explicit InputEvent(const nector_input_event& event)
        : type(static_cast<InputEventType>(event.type)), key(event.key),
          button(event.button), modifiers(event.modifiers), touch_id(event.touch_id),
          x(event.x), y(event.y), dx(event.dx), dy(event.dy),
          pressure(event.pressure), width(event.width), height(event.height),
          text(event.text) {}

    nector_input_event toNative() const {
        nector_input_event event{};
        event.type = static_cast<nector_input_event_type>(type);
        event.key = key;
        event.button = button;
        event.modifiers = modifiers;
        event.touch_id = touch_id;
        event.x = x;
        event.y = y;
        event.dx = dx;
        event.dy = dy;
        event.pressure = pressure;
        event.width = width;
        event.height = height;
        const std::size_t count = std::min(text.size(), sizeof(event.text) - 1U);
        std::memcpy(event.text, text.data(), count);
        event.text[count] = '\0';
        return event;
    }
};

class Input : public detail::ResourceHandle<nector_input_handle, nectorDestroyInput> {
    using Base = detail::ResourceHandle<nector_input_handle, nectorDestroyInput>;

public:
    Input() { detail::check(nectorCreateInput(&handle_)); }
    Input(Input&&) noexcept = default;
    Input& operator=(Input&&) noexcept = default;

    void push(const InputEvent& event) const {
        const nector_input_event native_event = event.toNative();
        detail::check(nectorInputPush(handle_, &native_event));
    }

    void pushAt(const InputEvent& event, std::int64_t timestamp_ms) const {
        const nector_input_event native_event = event.toNative();
        detail::check(nectorInputPushAt(handle_, &native_event, timestamp_ms));
    }

    InputEvent poll() const {
        nector_input_event event{};
        detail::check(nectorInputPoll(handle_, &event));
        return InputEvent(event);
    }

    InputEvent peek() const {
        nector_input_event event{};
        detail::check(nectorInputPeek(handle_, &event));
        return InputEvent(event);
    }

    void clear() const { detail::check(nectorInputClear(handle_)); }

    bool isKeyDown(std::int32_t key) const {
        std::int32_t down = 0;
        detail::check(nectorInputIsKeyDown(handle_, key, &down));
        return down != 0;
    }

    bool isKeyDown(Key key) const {
        return isKeyDown(static_cast<std::int32_t>(key));
    }

    bool isMouseDown(std::int32_t button) const {
        std::int32_t down = 0;
        detail::check(nectorInputIsMouseDown(handle_, button, &down));
        return down != 0;
    }

    Point mousePosition() const {
        Point position{};
        detail::check(nectorInputGetMousePosition(handle_, &position.x, &position.y));
        return position;
    }

    Point wheelDelta() const {
        Point delta{};
        detail::check(nectorInputGetWheelDelta(handle_, &delta.x, &delta.y));
        return delta;
    }

    std::int32_t touchCount() const {
        std::int32_t count = 0;
        detail::check(nectorInputGetTouchCount(handle_, &count));
        return count;
    }

    InputEvent touch(std::int32_t index) const {
        nector_input_event event{};
        detail::check(nectorInputGetTouch(handle_, index, &event));
        return InputEvent(event);
    }

    void injectKey(std::int32_t key, bool down, std::uint32_t modifiers = 0) const {
        detail::check(nectorInputInjectKey(handle_, key, down ? 1 : 0, modifiers));
    }

    void injectKey(Key key, bool down, std::uint32_t modifiers = 0) const {
        injectKey(static_cast<std::int32_t>(key), down, modifiers);
    }

    void injectKeyAt(std::int32_t key, bool down, std::uint32_t modifiers,
                     std::int64_t timestamp_ms) const {
        detail::check(nectorInputInjectKeyAt(handle_, key, down ? 1 : 0, modifiers,
                                             timestamp_ms));
    }

    void injectText(const std::string& utf8_text) const {
        detail::check(nectorInputInjectText(handle_, utf8_text.c_str()));
    }

    void injectMouse(InputEventType type, float x, float y,
                     std::int32_t button = 0) const {
        detail::check(nectorInputInjectMouse(handle_,
                                             static_cast<nector_input_event_type>(type),
                                             x, y, button));
    }

    void injectTouch(InputEventType type, std::int64_t touch_id, float x,
                     float y, float pressure = 1.0F) const {
        detail::check(nectorInputInjectTouch(handle_,
                                             static_cast<nector_input_event_type>(type),
                                             touch_id, x, y, pressure));
    }

private:
    explicit Input(nector_input_handle handle, bool owns) : Base(handle, owns) {}

    static Input borrow(nector_input_handle handle) {
        return Input(handle, false);
    }

    friend class Window;
};

class NKeystroke {
public:
    enum class signal : int {
        Unknown = nector_key_unknown,
        Backspace = nector_key_backspace,
        Tab = nector_key_tab,
        Enter = nector_key_enter,
        Escape = nector_key_escape,
        Space = nector_key_space,
        Num0 = nector_key_0,
        Num1 = nector_key_1,
        Num2 = nector_key_2,
        Num3 = nector_key_3,
        Num4 = nector_key_4,
        Num5 = nector_key_5,
        Num6 = nector_key_6,
        Num7 = nector_key_7,
        Num8 = nector_key_8,
        Num9 = nector_key_9,
        A = nector_key_a,
        B = nector_key_b,
        C = nector_key_c,
        D = nector_key_d,
        E = nector_key_e,
        F = nector_key_f,
        G = nector_key_g,
        H = nector_key_h,
        I = nector_key_i,
        J = nector_key_j,
        K = nector_key_k,
        L = nector_key_l,
        M = nector_key_m,
        N = nector_key_n,
        O = nector_key_o,
        P = nector_key_p,
        Q = nector_key_q,
        R = nector_key_r,
        S = nector_key_s,
        T = nector_key_t,
        U = nector_key_u,
        V = nector_key_v,
        W = nector_key_w,
        X = nector_key_x,
        Y = nector_key_y,
        Z = nector_key_z,
        Left = nector_key_left,
        Right = nector_key_right,
        Up = nector_key_up,
        Down = nector_key_down,
        Home = nector_key_home,
        End = nector_key_end,
        Delete = nector_key_delete,
        Shift = nector_key_shift,
        Control = nector_key_control,
        Alt = nector_key_alt
    };

    explicit NKeystroke(const Input& input) noexcept : handle_(input.nativeHandle()) {}

    std::int32_t state(signal key) const {
        std::int32_t value = 0;
        detail::check(nectorInputGetKeyState(handle_, static_cast<std::int32_t>(key), &value));
        return value;
    }

    std::int32_t stateAt(signal key, std::int64_t timestamp_ms) const {
        std::int32_t value = 0;
        detail::check(nectorInputGetKeyStateAt(handle_, static_cast<std::int32_t>(key),
                                               timestamp_ms, &value));
        return value;
    }

private:
    nector_input_handle handle_;
};

enum class NodeType : int {
    Window = nector_node_window,
    Column = nector_node_column,
    Row = nector_node_row,
    Label = nector_node_label,
    Button = nector_node_button,
    LineEdit = nector_node_line_edit,
    CheckBox = nector_node_check_box,
    Slider = nector_node_slider,
    ProgressBar = nector_node_progress_bar,
    Spacer = nector_node_spacer,
    Canvas = nector_node_canvas,
    Image = nector_node_image
};

enum class NodeProperty : int {
    Text = nector_property_text,
    Title = nector_property_title,
    Placeholder = nector_property_placeholder,
    Source = nector_property_source,
    Width = nector_property_width,
    Height = nector_property_height,
    Padding = nector_property_padding,
    Gap = nector_property_gap,
    Flex = nector_property_flex,
    FontSize = nector_property_font_size,
    Value = nector_property_value,
    Minimum = nector_property_minimum,
    Maximum = nector_property_maximum,
    Step = nector_property_step,
    Caret = nector_property_caret,
    CornerRadius = nector_property_corner_radius,
    ContentOffsetX = nector_property_content_offset_x,
    ContentOffsetY = nector_property_content_offset_y,
    ScrollStep = nector_property_scroll_step,
    Layout = nector_property_layout,
    Visible = nector_property_visible,
    Enabled = nector_property_enabled,
    Checked = nector_property_checked,
    WordWrap = nector_property_word_wrap,
    NoShrink = nector_property_no_shrink,
    Glass = nector_property_glass,
    Background = nector_property_background,
    Foreground = nector_property_foreground,
    Border = nector_property_border,
    Accent = nector_property_accent
};

class Node;

namespace detail {
Node interfaceNodeFromHandle(nector_node_handle handle);
}

class Node {
public:
    Node() noexcept = default;
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    Node(Node&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    Node& operator=(Node&& other) noexcept {
        if (this != &other) {
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    nector_node_handle nativeHandle() const noexcept { return handle_; }
    explicit operator bool() const noexcept { return handle_ != nullptr; }

    void setString(NodeProperty property, const std::string& value) const {
        detail::check(nectorSceneSetNodeString(
            handle_, static_cast<nector_node_property>(property), value.c_str()));
    }

    void setNumber(NodeProperty property, double value) const {
        detail::check(nectorSceneSetNodeNumber(
            handle_, static_cast<nector_node_property>(property), value));
    }

    void setBool(NodeProperty property, bool value) const {
        detail::check(nectorSceneSetNodeBool(
            handle_, static_cast<nector_node_property>(property), value ? 1 : 0));
    }

    void setColor(NodeProperty property, Color value) const {
        detail::check(nectorSceneSetNodeColor(
            handle_, static_cast<nector_node_property>(property), value));
    }

    std::string string(NodeProperty property) const {
        const char* value = nullptr;
        detail::check(nectorSceneGetNodeString(
            handle_, static_cast<nector_node_property>(property), &value));
        return value != nullptr ? std::string(value) : std::string();
    }

    double number(NodeProperty property) const {
        double value = 0.0;
        detail::check(nectorSceneGetNodeNumber(
            handle_, static_cast<nector_node_property>(property), &value));
        return value;
    }

    bool boolean(NodeProperty property) const {
        std::int32_t value = 0;
        detail::check(nectorSceneGetNodeBool(
            handle_, static_cast<nector_node_property>(property), &value));
        return value != 0;
    }

    Color color(NodeProperty property) const {
        Color value{};
        detail::check(nectorSceneGetNodeColor(
            handle_, static_cast<nector_node_property>(property), &value));
        return value;
    }

    NodeType type() const {
        nector_node_type value = nector_node_window;
        detail::check(nectorSceneGetNodeType(handle_, &value));
        return static_cast<NodeType>(value);
    }

    std::string id() const {
        const char* value = nullptr;
        detail::check(nectorSceneGetNodeId(handle_, &value));
        return value != nullptr ? std::string(value) : std::string();
    }

    Rect bounds() const {
        Rect value{};
        detail::check(nectorSceneGetNodeBounds(handle_, &value));
        return value;
    }

    void setImage(const Image& image) const {
        detail::check(nectorSceneSetNodeImage(handle_, image.nativeHandle()));
    }

private:
    explicit Node(nector_node_handle handle) noexcept : handle_(handle) {}

    nector_node_handle handle_ = nullptr;

    friend class Scene;
    friend struct UiEvent;
    friend Node detail::interfaceNodeFromHandle(nector_node_handle handle);
};

namespace detail {
inline Node interfaceNodeFromHandle(nector_node_handle handle) {
    return Node(handle);
}
}

enum class UiEventType : int {
    None = nector_ui_none,
    Click = nector_ui_click,
    TextChanged = nector_ui_text_changed,
    ValueChanged = nector_ui_value_changed,
    Toggled = nector_ui_toggled,
    Paint = nector_ui_paint,
    Focus = nector_ui_focus,
    Blur = nector_ui_blur,
    Custom = nector_ui_custom,
    AnimationFinished = nector_ui_animation_finished,
    Quit = nector_ui_quit,
    Scroll = nector_ui_scroll
};

struct UiEvent {
    UiEventType type = UiEventType::None;
    Node node;
    std::string node_id;
    std::string text;
    std::string name;
    double value = 0.0;
    Painter painter;
    Rect bounds{};

    UiEvent() = default;

    explicit UiEvent(const nector_ui_event& event)
        : type(static_cast<UiEventType>(event.type)), node(event.node),
          node_id(event.node_id != nullptr ? event.node_id : ""),
          text(event.text != nullptr ? event.text : ""),
          name(event.name != nullptr ? event.name : ""), value(event.value),
          painter(Painter::borrow(event.painter)), bounds(event.bounds) {}
};

using EventCallback = std::function<void(const UiEvent&)>;

namespace detail {

struct CallbackState {
    EventCallback callback;
    bool registered = false;
};

inline void sceneCallbackTrampoline(const nector_ui_event* native_event,
                                    void* user_data) noexcept {
    auto* state = static_cast<CallbackState*>(user_data);
    if (state == nullptr || !state->callback || native_event == nullptr) {
        return;
    }
    try {
        const UiEvent event(*native_event);
        EventCallback callback = state->callback;
        callback(event);
    } catch (const std::exception& exception) {
        nectorCoreSetError(nector_callback_error, exception.what());
    } catch (...) {
        nectorCoreSetError(nector_callback_error, "Nector scene callback threw an unknown exception");
    }
}

} // namespace detail

class Scene : public detail::ResourceHandle<nector_scene_handle, nectorDestroyScene> {
    using Base = detail::ResourceHandle<nector_scene_handle, nectorDestroyScene>;

public:
    Scene() noexcept = default;

    Scene(const std::string& title, std::int32_t width, std::int32_t height) {
        detail::check(nectorCreateScene(title.c_str(), width, height, &handle_));
    }

    Scene(Scene&&) noexcept = default;

    Scene& operator=(Scene&& other) noexcept {
        if (this != &other) {
            clearCallback();
            Base::operator=(std::move(other));
            callback_state_ = std::move(other.callback_state_);
        }
        return *this;
    }

    ~Scene() { clearCallback(); }

    Node root() const {
        nector_node_handle node = nullptr;
        detail::check(nectorSceneGetRoot(handle_, &node));
        return Node(node);
    }

    Node addNode(const Node& parent, NodeType type,
                 const std::string& id) const {
        nector_node_handle node = nullptr;
        detail::check(nectorSceneAddNode(handle_, parent.nativeHandle(),
                                         static_cast<nector_node_type>(type),
                                         id.c_str(), &node));
        return Node(node);
    }

    Node findNode(const std::string& id) const {
        nector_node_handle node = nullptr;
        detail::check(nectorSceneFindNode(handle_, id.c_str(), &node));
        return Node(node);
    }

    void removeNode(Node& node) const {
        detail::check(nectorSceneRemoveNode(handle_, node.nativeHandle()));
        node.handle_ = nullptr;
    }

    void setFont(const Font& font) const {
        detail::check(nectorSceneSetFont(handle_, font.nativeHandle()));
    }

    void setEventCallback(EventCallback callback) {
        if (callback && callback_state_ == nullptr) {
            callback_state_ = std::make_unique<detail::CallbackState>();
        }
        detail::CallbackState* state = callback_state_.get();
        if (state != nullptr) {
            state->callback = std::move(callback);
        }
        detail::check(nectorSceneSetEventCallback(
            handle_, state != nullptr && state->callback
                         ? &detail::sceneCallbackTrampoline
                         : nullptr,
            state != nullptr && state->callback ? state : nullptr));
        if (state != nullptr) {
            state->registered = static_cast<bool>(state->callback);
        }
    }

    void emitEvent(const std::string& name, const std::string& text = std::string(),
                   double value = 0.0) const {
        detail::check(nectorSceneEmit(handle_, name.c_str(), text.c_str(), value));
    }

    void bindAnimation(const Animation& animation, std::int32_t channel,
                       const Node& node, NodeProperty property,
                       std::int32_t finished_action_id = 0) const {
        detail::check(nectorSceneBindAnimation(
            handle_, animation.nativeHandle(), channel, node.nativeHandle(),
            static_cast<nector_node_property>(property), finished_action_id));
    }

    void bindAnimation(const detail::FrameAnimation& animation, std::int32_t channel,
                       const Node& node, NodeProperty property,
                       std::int32_t finished_action_id = 0) const {
        detail::check(nectorSceneBindAnimation(
            handle_, animation.nativeHandle(), channel, node.nativeHandle(),
            static_cast<nector_node_property>(property), finished_action_id));
    }

    void tick(double delta_ms) const {
        detail::check(nectorSceneTick(handle_, delta_ms));
    }

    void layout(std::int32_t width, std::int32_t height) const {
        detail::check(nectorSceneLayout(handle_, width, height));
    }

    void draw(const Painter& painter) const {
        detail::check(nectorSceneDraw(handle_, painter.nativeHandle()));
    }

    void handleInput(const InputEvent& event) const {
        const nector_input_event native_event = event.toNative();
        detail::check(nectorSceneHandleInput(handle_, &native_event));
    }

    Size size() const {
        Size result;
        detail::check(nectorSceneGetSize(handle_, &result.width, &result.height));
        return result;
    }

    std::string title() const {
        const char* value = nullptr;
        detail::check(nectorSceneGetTitle(handle_, &value));
        return value != nullptr ? std::string(value) : std::string();
    }

private:
    Scene(nector_scene_handle handle, bool owns) : Base(handle, owns) {}

    static Scene borrow(nector_scene_handle handle) {
        return Scene(handle, false);
    }

    void clearCallback() noexcept {
        if (callback_state_ == nullptr || !callback_state_->registered) {
            return;
        }
        if (handle_ != nullptr &&
            nectorSceneSetEventCallback(handle_, nullptr, nullptr) == nector_ok) {
            callback_state_->registered = false;
            return;
        }

        // A failed unregistration must not leave C with a dangling user_data pointer.
        (void)callback_state_.release();
    }

    std::unique_ptr<detail::CallbackState> callback_state_;

    friend class Window;
    friend class CdslPackage;
};

class NInterface : public detail::ResourceHandle<nector_interface_context_handle, nectorInterfaceDestroyContext> {
    using Base = detail::ResourceHandle<nector_interface_context_handle, nectorInterfaceDestroyContext>;

public:
    // The Scene is borrowed and must outlive this context and all of its controls.
    explicit NInterface(Scene& scene) {
        detail::check(nectorInterfaceCreateContext(scene.nativeHandle(), &handle_));
    }

    NInterface(NInterface&&) noexcept = default;
    NInterface& operator=(NInterface&&) noexcept = default;

    void layout(std::int32_t width, std::int32_t height) const {
        detail::check(nectorInterfaceLayout(handle_, width, height));
    }

    void refresh() const { detail::check(nectorInterfaceRefresh(handle_)); }

    void setEventCallback(EventCallback callback) {
        if (callback && callback_state_ == nullptr) {
            callback_state_ = std::make_unique<detail::CallbackState>();
        }
        detail::CallbackState* state = callback_state_.get();
        if (state != nullptr) state->callback = std::move(callback);
        detail::check(nectorInterfaceSetEventCallback(
            handle_, state != nullptr && state->callback ? &detail::sceneCallbackTrampoline : nullptr,
            state != nullptr && state->callback ? state : nullptr));
    }

    class Control {
    public:
        Control(NInterface& context, NodeType type, const std::string& id) {
            detail::check(nectorInterfaceCreateControl(context.nativeHandle(), nullptr,
                static_cast<nector_node_type>(type), id.c_str(), &handle_));
        }

        Control(NInterface& context, const Control& parent, NodeType type, const std::string& id) {
            detail::check(nectorInterfaceCreateControl(context.nativeHandle(), parent.nativeHandle(),
                static_cast<nector_node_type>(type), id.c_str(), &handle_));
        }

        Control(const Control&) = delete;
        Control& operator=(const Control&) = delete;

        ~Control() {
            if (handle_ != nullptr) {
                (void)nectorInterfaceControlSetEventCallback(handle_, nullptr, nullptr);
            }
        }

        nector_interface_control_handle nativeHandle() const noexcept { return handle_; }
        explicit operator bool() const noexcept { return handle_ != nullptr; }

        Node node() const {
            nector_node_handle node = nullptr;
            detail::check(nectorInterfaceControlGetNode(handle_, &node));
            return detail::interfaceNodeFromHandle(node);
        }

        void setString(NodeProperty property, const std::string& value) const {
            detail::check(nectorInterfaceControlSetString(handle_,
                static_cast<nector_node_property>(property), value.c_str()));
        }

        void setNumber(NodeProperty property, double value) const {
            detail::check(nectorInterfaceControlSetNumber(handle_,
                static_cast<nector_node_property>(property), value));
        }

        void setBool(NodeProperty property, bool value) const {
            detail::check(nectorInterfaceControlSetBool(handle_,
                static_cast<nector_node_property>(property), value ? 1 : 0));
        }

        void setColor(NodeProperty property, Color value) const {
            detail::check(nectorInterfaceControlSetColor(handle_,
                static_cast<nector_node_property>(property), value));
        }

        std::string string(NodeProperty property) const {
            const char* value = nullptr;
            detail::check(nectorInterfaceControlGetString(handle_,
                static_cast<nector_node_property>(property), &value));
            return value != nullptr ? std::string(value) : std::string();
        }

        double number(NodeProperty property) const {
            double value = 0.0;
            detail::check(nectorInterfaceControlGetNumber(handle_,
                static_cast<nector_node_property>(property), &value));
            return value;
        }

        bool boolean(NodeProperty property) const {
            std::int32_t value = 0;
            detail::check(nectorInterfaceControlGetBool(handle_,
                static_cast<nector_node_property>(property), &value));
            return value != 0;
        }

        Color color(NodeProperty property) const {
            Color value{};
            detail::check(nectorInterfaceControlGetColor(handle_,
                static_cast<nector_node_property>(property), &value));
            return value;
        }

        void setEventCallback(EventCallback callback) {
            if (callback && callback_state_ == nullptr) {
                callback_state_ = std::make_unique<detail::CallbackState>();
            }
            detail::CallbackState* state = callback_state_.get();
            if (state != nullptr) state->callback = std::move(callback);
            detail::check(nectorInterfaceControlSetEventCallback(
                handle_, state != nullptr && state->callback ? &detail::sceneCallbackTrampoline : nullptr,
                state != nullptr && state->callback ? state : nullptr));
        }

        void destroy() {
            if (handle_ == nullptr) return;
            detail::check(nectorInterfaceControlSetEventCallback(handle_, nullptr, nullptr));
            detail::check(nectorInterfaceDestroyControl(handle_));
            handle_ = nullptr;
        }

    protected:
        Control() noexcept = default;

        nector_interface_control_handle handle_ = nullptr;

    private:
        std::unique_ptr<detail::CallbackState> callback_state_;
    };

    class Row : public Control {
    public:
        enum class Part : int {
            Node = nector_interface_row_node,
            Viewport = nector_interface_row_viewport,
            Content = nector_interface_row_content,
            Backward = nector_interface_row_backward,
            Forward = nector_interface_row_forward
        };

        class LayoutProperty {
        public:
            LayoutProperty& operator=(std::int32_t value) {
                detail::check(nectorInterfaceRowSetLayout(*handle_, value));
                return *this;
            }

            operator std::int32_t() const {
                std::int32_t value = 0;
                detail::check(nectorInterfaceRowGetLayout(*handle_, &value));
                return value;
            }

        private:
            explicit LayoutProperty(const nector_interface_control_handle* handle) noexcept : handle_(handle) {}

            const nector_interface_control_handle* handle_;

            friend class Row;
        };

        Row(NInterface& context, const std::string& id) : layout(&handle_) {
            detail::check(nectorInterfaceCreateRow(context.nativeHandle(), nullptr, id.c_str(), &handle_));
        }

        Row(NInterface& context, const Control& parent, const std::string& id) : layout(&handle_) {
            detail::check(nectorInterfaceCreateRow(context.nativeHandle(), parent.nativeHandle(), id.c_str(), &handle_));
        }

        void setScroll(double value) const {
            detail::check(nectorInterfaceRowSetScroll(handle_, value));
        }

        double scroll() const {
            double value = 0.0;
            detail::check(nectorInterfaceRowGetScroll(handle_, &value));
            return value;
        }

        void scrollBy(double delta) const {
            detail::check(nectorInterfaceRowScrollBy(handle_, delta));
        }

        double maxScroll() const {
            double value = 0.0;
            detail::check(nectorInterfaceRowGetMaxScroll(handle_, &value));
            return value;
        }

        Node part(Part which) const {
            nector_node_handle node = nullptr;
            detail::check(nectorInterfaceRowGetPart(handle_, static_cast<nector_interface_row_part>(which), &node));
            return detail::interfaceNodeFromHandle(node);
        }

        LayoutProperty layout;
    };

    class Button : public Control {
    public:
        class GlassProperty {
        public:
            GlassProperty& operator=(bool value) {
                detail::check(nectorInterfaceControlSetBool(*handle_, nector_property_glass, value ? 1 : 0));
                return *this;
            }

            operator bool() const {
                std::int32_t value = 0;
                detail::check(nectorInterfaceControlGetBool(*handle_, nector_property_glass, &value));
                return value != 0;
            }

        private:
            explicit GlassProperty(const nector_interface_control_handle* handle) noexcept : handle_(handle) {}

            const nector_interface_control_handle* handle_;

            friend class Button;
        };

        Button(NInterface& context, const std::string& id, const std::string& text) : glass(&handle_) {
            detail::check(nectorInterfaceCreateButton(context.nativeHandle(), nullptr,
                                                      id.c_str(), text.c_str(), &handle_));
        }

        Button(NInterface& context, const Control& parent,
               const std::string& id, const std::string& text) : glass(&handle_) {
            detail::check(nectorInterfaceCreateButton(context.nativeHandle(), parent.nativeHandle(),
                                                      id.c_str(), text.c_str(), &handle_));
        }

        GlassProperty glass;
    };

private:
    std::unique_ptr<detail::CallbackState> callback_state_;
};

class Sound : public detail::ResourceHandle<nector_sound_handle, nectorAudioUnload> {
    using Base = detail::ResourceHandle<nector_sound_handle, nectorAudioUnload>;

public:
    Sound() noexcept = default;
    Sound(Sound&&) noexcept = default;
    Sound& operator=(Sound&&) noexcept = default;

    void play() const { detail::check(nectorAudioPlay(handle_)); }
    void pause() const { detail::check(nectorAudioPause(handle_)); }
    void stop() const { detail::check(nectorAudioStop(handle_)); }

    void setVolume(float volume) const {
        detail::check(nectorAudioSetVolume(handle_, volume));
    }

    float volume() const {
        float value = 0.0F;
        detail::check(nectorAudioGetVolume(handle_, &value));
        return value;
    }

    void setLooping(bool looping) const {
        detail::check(nectorAudioSetLooping(handle_, looping ? 1 : 0));
    }

    bool isPlaying() const {
        std::int32_t value = 0;
        detail::check(nectorAudioIsPlaying(handle_, &value));
        return value != 0;
    }

    void seek(double seconds) const {
        detail::check(nectorAudioSeek(handle_, seconds));
    }

    double duration() const {
        double value = 0.0;
        detail::check(nectorAudioGetDuration(handle_, &value));
        return value;
    }

private:
    explicit Sound(nector_sound_handle handle) : Base(handle) {}

    friend class Audio;
};

class Audio : public detail::ResourceHandle<nector_audio_handle, nectorDestroyAudio> {
    using Base = detail::ResourceHandle<nector_audio_handle, nectorDestroyAudio>;

public:
    Audio() { detail::check(nectorCreateAudio(&handle_)); }
    Audio(Audio&&) noexcept = default;
    Audio& operator=(Audio&&) noexcept = default;

    Sound load(const std::string& path) const {
        nector_sound_handle sound = nullptr;
        detail::check(nectorAudioLoad(handle_, path.c_str(), &sound));
        return Sound(sound);
    }

    Sound loadMemory(const std::uint8_t* data, std::size_t size) const {
        nector_sound_handle sound = nullptr;
        detail::check(nectorAudioLoadMemory(handle_, data, size, &sound));
        return Sound(sound);
    }

    Sound loadMemory(const std::vector<std::uint8_t>& data) const {
        return loadMemory(data.data(), data.size());
    }
};

class Window;

class Application : public detail::ResourceHandle<nector_application_handle, nectorDestroyApplication> {
    using Base = detail::ResourceHandle<nector_application_handle, nectorDestroyApplication>;

public:
    Application() { detail::check(nectorCreateApplication(&handle_)); }
    Application(Application&&) noexcept = default;
    Application& operator=(Application&&) noexcept = default;

    Window createWindow(Scene& scene);

    void processEvents() const { detail::check(nectorProcessEvents(handle_)); }
    void run() const { detail::check(nectorRun(handle_)); }
    void quit() const { detail::check(nectorQuit(handle_)); }

    bool isRunning() const {
        std::int32_t value = 0;
        detail::check(nectorApplicationIsRunning(handle_, &value));
        return value != 0;
    }

    static std::string platformName() {
        const char* value = nectorPlatformName();
        return value != nullptr ? std::string(value) : std::string();
    }
};

class Window : public detail::ResourceHandle<nector_window_handle, nectorDestroyWindow> {
    using Base = detail::ResourceHandle<nector_window_handle, nectorDestroyWindow>;

public:
    // The Scene is borrowed and must remain alive until this Window is destroyed.
    Window(Application& application, Scene& scene) {
        detail::check(nectorCreateWindow(application.nativeHandle(),
                                         scene.nativeHandle(), &handle_));
    }

    Window(Window&&) noexcept = default;
    Window& operator=(Window&&) noexcept = default;

    void show() const { detail::check(nectorWindowShow(handle_)); }
    void hide() const { detail::check(nectorWindowHide(handle_)); }
    void requestClose() const { detail::check(nectorWindowRequestClose(handle_)); }

    Input input() const {
        nector_input_handle input = nullptr;
        detail::check(nectorWindowGetInput(handle_, &input));
        return Input::borrow(input);
    }

    Scene scene() const {
        nector_scene_handle scene = nullptr;
        detail::check(nectorWindowGetScene(handle_, &scene));
        return Scene::borrow(scene);
    }

    Size size() const {
        Size result;
        detail::check(nectorWindowGetSize(handle_, &result.width, &result.height));
        return result;
    }

    void setTitle(const std::string& title) const {
        detail::check(nectorWindowSetTitle(handle_, title.c_str()));
    }

    void render() const { detail::check(nectorWindowRender(handle_)); }
};

inline Window Application::createWindow(Scene& scene) {
    return Window(*this, scene);
}

class CdslPackage : public detail::ResourceHandle<nector_package_handle, nectorCdslDestroyPackage> {
    using Base = detail::ResourceHandle<nector_package_handle, nectorCdslDestroyPackage>;

public:
    CdslPackage() noexcept = default;

    explicit CdslPackage(const std::string& path) {
        detail::check(nectorCdslLoadFile(path.c_str(), &handle_));
    }

    CdslPackage(CdslPackage&&) noexcept = default;

    CdslPackage& operator=(CdslPackage&& other) noexcept {
        if (this != &other) {
            clearCallback();
            Base::operator=(std::move(other));
            callback_state_ = std::move(other.callback_state_);
        }
        return *this;
    }

    ~CdslPackage() { clearCallback(); }

    static std::vector<std::uint8_t> compileText(const std::string& utf8_source) {
        std::uint8_t* data = nullptr;
        std::size_t size = 0;
        detail::check(nectorCdslCompileText(utf8_source.c_str(), &data, &size));
        std::unique_ptr<std::uint8_t, void (*)(std::uint8_t*)> buffer(
            data, &nectorCdslFreeBuffer);
        if (size == 0) {
            return {};
        }
        return std::vector<std::uint8_t>(buffer.get(), buffer.get() + size);
    }

    static void compileFile(const std::string& input_path,
                            const std::string& output_path) {
        detail::check(nectorCdslCompileFile(input_path.c_str(), output_path.c_str()));
    }

    static CdslPackage load(const std::string& path) {
        return CdslPackage(path);
    }

    static CdslPackage loadMemory(const std::uint8_t* data, std::size_t size) {
        nector_package_handle handle = nullptr;
        detail::check(nectorCdslLoadMemory(data, size, &handle));
        return CdslPackage(handle);
    }

    static CdslPackage loadMemory(const std::vector<std::uint8_t>& data) {
        return loadMemory(data.data(), data.size());
    }

    Scene scene() const {
        nector_scene_handle scene = nullptr;
        detail::check(nectorCdslGetScene(handle_, &scene));
        return Scene::borrow(scene);
    }

    void call(const std::string& action_id) const {
        detail::check(nectorCdslCall(handle_, action_id.c_str()));
    }

    bool hasAction(const std::string& action_id) const {
        std::int32_t found = 0;
        detail::check(nectorCdslHasAction(handle_, action_id.c_str(), &found));
        return found != 0;
    }

    std::int32_t actionCount() const {
        std::int32_t count = 0;
        detail::check(nectorCdslGetActionCount(handle_, &count));
        return count;
    }

    std::string actionId(std::int32_t index) const {
        const char* value = nullptr;
        detail::check(nectorCdslGetActionId(handle_, index, &value));
        return value != nullptr ? std::string(value) : std::string();
    }

    void setEventCallback(EventCallback callback) {
        if (callback && callback_state_ == nullptr) {
            callback_state_ = std::make_unique<detail::CallbackState>();
        }
        detail::CallbackState* state = callback_state_.get();
        if (state != nullptr) {
            state->callback = std::move(callback);
        }
        detail::check(nectorCdslSetEventCallback(
            handle_, state != nullptr && state->callback
                         ? &detail::sceneCallbackTrampoline
                         : nullptr,
            state != nullptr && state->callback ? state : nullptr));
        if (state != nullptr) {
            state->registered = static_cast<bool>(state->callback);
        }
    }

private:
    explicit CdslPackage(nector_package_handle handle) : Base(handle) {}

    void clearCallback() noexcept {
        if (callback_state_ == nullptr || !callback_state_->registered) {
            return;
        }
        if (handle_ != nullptr &&
            nectorCdslSetEventCallback(handle_, nullptr, nullptr) == nector_ok) {
            callback_state_->registered = false;
            return;
        }

        // A failed unregistration must not leave C with a dangling user_data pointer.
        (void)callback_state_.release();
    }

    std::unique_ptr<detail::CallbackState> callback_state_;
};

} // namespace nector

using NKeystroke = nector::NKeystroke;
using NImation = nector::NImation;
using NInterface = nector::NInterface;

#endif // NECTOR_NECTOR_HPP
