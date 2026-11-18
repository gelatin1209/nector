#if defined(__APPLE__)

#include "backend.h"

#include <Cocoa/Cocoa.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

namespace {

uint32_t cocoaModifiers(NSEventModifierFlags flags) {
    uint32_t modifiers = 0;
    if ((flags & NSEventModifierFlagShift) != 0) modifiers |= platform_shift_modifier;
    if ((flags & NSEventModifierFlagControl) != 0) modifiers |= platform_control_modifier;
    if ((flags & NSEventModifierFlagOption) != 0) modifiers |= platform_alt_modifier;
    return modifiers;
}

int32_t cocoaKey(NSEvent* event) {
    switch (event.keyCode) {
        case 36: return nector_key_enter;
        case 48: return nector_key_tab;
        case 51: return nector_key_backspace;
        case 53: return nector_key_escape;
        case 115: return nector_key_home;
        case 117: return nector_key_delete;
        case 119: return nector_key_end;
        case 123: return nector_key_left;
        case 124: return nector_key_right;
        case 125: return nector_key_down;
        case 126: return nector_key_up;
        case 56:
        case 60: return nector_key_shift;
        case 59:
        case 62: return nector_key_control;
        case 58:
        case 61: return nector_key_alt;
        case 18: return '1';
        case 19: return '2';
        case 20: return '3';
        case 21: return '4';
        case 23: return '5';
        case 22: return '6';
        case 26: return '7';
        case 28: return '8';
        case 25: return '9';
        case 29: return '0';
        default: break;
    }
    NSString* characters = event.charactersIgnoringModifiers;
    if (characters.length != 1) return nector_key_unknown;
    const unichar character = [characters characterAtIndex:0];
    if (character == ' ') return nector_key_space;
    if (character >= 'a' && character <= 'z') return static_cast<int32_t>('A' + character - 'a');
    if ((character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9')) {
        return static_cast<int32_t>(character);
    }
    return nector_key_unknown;
}

class CocoaWindow;

} // namespace

@interface NectorView : NSView
@property(nonatomic, assign) nector_window_handle nectorWindow;
- (void)dispatchKey:(NSEvent*)event down:(BOOL)down;
@end

@interface NectorWindowDelegate : NSObject<NSWindowDelegate>
@property(nonatomic, assign) nector_window_handle nectorWindow;
@end

namespace {

class CocoaBackend final : public PlatformBackend {
public:
    explicit CocoaBackend(nector_application_handle application) : application_(application) {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];
    }

    nector_status createWindow(nector_window_handle window, std::unique_ptr<PlatformWindow>& out_window) override;

    nector_status processEvents(bool wait) override {
        @autoreleasepool {
            if (wait && application_->running) {
                NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                    untilDate:[NSDate distantFuture]
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES];
                if (event != nullptr) [NSApp sendEvent:event];
            }
            for (;;) {
                NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                    untilDate:[NSDate distantPast]
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES];
                if (event == nullptr) break;
                [NSApp sendEvent:event];
            }
            [NSApp updateWindows];
            return nector_ok;
        }
    }

    void wakeUp() override {
        NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                            location:NSZeroPoint
                                       modifierFlags:0
                                           timestamp:0
                                        windowNumber:0
                                             context:nil
                                             subtype:0
                                               data1:0
                                               data2:0];
        if (event != nullptr) [NSApp postEvent:event atStart:NO];
    }

private:
    nector_application_handle application_;
};

class CocoaWindow final : public PlatformWindow {
public:
    explicit CocoaWindow(nector_window_handle window) : window_(window) {}

    ~CocoaWindow() override {
        closeNative();
    }

    nector_status create() {
        @autoreleasepool {
            const NSRect frame = NSMakeRect(0, 0, window_->width, window_->height);
            const NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                            NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
            native_window_ = [[NSWindow alloc] initWithContentRect:frame
                                                        styleMask:style
                                                          backing:NSBackingStoreBuffered
                                                            defer:NO];
            if (native_window_ == nullptr) return nectorFail(nector_backend_error, "NSWindow creation failed");
            view_ = [[NectorView alloc] initWithFrame:frame];
            if (view_ == nullptr) return nectorFail(nector_out_of_memory, "NectorView creation failed");
            view_.nectorWindow = window_;
            view_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
            [view_ setAcceptsTouchEvents:YES];
            delegate_ = [[NectorWindowDelegate alloc] init];
            if (delegate_ == nullptr) return nectorFail(nector_out_of_memory, "NectorWindowDelegate creation failed");
            delegate_.nectorWindow = window_;
            native_window_.delegate = delegate_;
            native_window_.contentView = view_;
            native_window_.releasedWhenClosed = NO;
            native_window_.acceptsMouseMovedEvents = YES;
            [native_window_ makeFirstResponder:view_];
            const char* title = nullptr;
            const nector_status status = nectorSceneGetTitle(window_->scene, &title);
            if (status != nector_ok) return status;
            return setTitle(title);
        }
    }

    nector_status show() override {
        if (native_window_ == nullptr) return nectorFail(nector_invalid_state, "native window is closed");
        [native_window_ makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        return nector_ok;
    }

    nector_status hide() override {
        if (native_window_ == nullptr) return nectorFail(nector_invalid_state, "native window is closed");
        [native_window_ orderOut:nil];
        return nector_ok;
    }

    nector_status requestClose() override {
        if (native_window_ == nullptr) return nectorFail(nector_invalid_state, "native window is closed");
        [native_window_ performClose:nil];
        return nector_ok;
    }

    nector_status setTitle(const char* title) override {
        if (native_window_ == nullptr) return nectorFail(nector_invalid_state, "native window is closed");
        NSString* native_title = [NSString stringWithUTF8String:title];
        if (native_title == nullptr) return nectorFail(nector_invalid_argument, "window title is not valid UTF-8");
        native_window_.title = native_title;
        return nector_ok;
    }

    nector_status present(const std::vector<uint8_t>& rgba, int32_t width, int32_t height) override {
        if (view_ == nullptr || width <= 0 || height <= 0) return nector_ok;
        const size_t pixel_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
        if (rgba.size() < pixel_size) return nectorFail(nector_invalid_argument, "RGBA pixel buffer is too small");
        @autoreleasepool {
            NSBitmapImageRep* representation = [[NSBitmapImageRep alloc]
                initWithBitmapDataPlanes:nullptr
                              pixelsWide:width
                              pixelsHigh:height
                           bitsPerSample:8
                         samplesPerPixel:4
                                hasAlpha:YES
                                isPlanar:NO
                          colorSpaceName:NSCalibratedRGBColorSpace
                            bitmapFormat:NSAlphaNonpremultipliedBitmapFormat
                             bytesPerRow:static_cast<NSInteger>(width) * 4
                            bitsPerPixel:32];
            if (representation == nullptr) return nectorFail(nector_out_of_memory, "NSBitmapImageRep creation failed");
            std::memcpy(representation.bitmapData, rgba.data(), pixel_size);
            NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
            if (image == nullptr) {
                [representation release];
                return nectorFail(nector_out_of_memory, "NSImage creation failed");
            }
            [image addRepresentation:representation];
            BOOL locked = NO;
            if ([NSGraphicsContext currentContext] == nullptr) {
                [view_ lockFocus];
                locked = YES;
            }
            [image drawInRect:view_.bounds
                     fromRect:NSZeroRect
                    operation:NSCompositingOperationCopy
                     fraction:1.0
               respectFlipped:YES
                        hints:nil];
            if (locked) {
                [view_ unlockFocus];
                [native_window_ flushWindow];
            }
            [image release];
            [representation release];
            return nector_ok;
        }
    }

    void closeNative() override {
        if (native_window_ == nullptr) return;
        NSWindow* native_window = native_window_;
        NectorView* view = view_;
        NectorWindowDelegate* delegate = delegate_;
        native_window_ = nullptr;
        view_ = nullptr;
        delegate_ = nullptr;
        native_window.delegate = nil;
        view.nectorWindow = nullptr;
        delegate.nectorWindow = nullptr;
        [native_window close];
        [native_window release];
        [view release];
        [delegate release];
    }

private:
    nector_window_handle window_;
    NSWindow* native_window_ = nullptr;
    NectorView* view_ = nullptr;
    NectorWindowDelegate* delegate_ = nullptr;
};

nector_status CocoaBackend::createWindow(nector_window_handle window, std::unique_ptr<PlatformWindow>& out_window) {
    std::unique_ptr<CocoaWindow> native(new CocoaWindow(window));
    const nector_status status = native->create();
    if (status != nector_ok) return status;
    out_window = std::move(native);
    return nector_ok;
}

void dispatchCocoaEvent(nector_window_handle window, NSEvent* native_event, nector_input_event& event,
                        bool key_repeat = false) {
    WindowGuard guard(window);
    event.modifiers = cocoaModifiers(native_event.modifierFlags);
    platformRecordEventStatus(window, platformDispatchInput(window, event, key_repeat));
}

NSPoint cocoaPoint(NectorView* view, NSEvent* event) {
    return [view convertPoint:event.locationInWindow fromView:nil];
}

void dispatchTouch(NectorView* view, NSSet* touches, NSEvent* event, nector_input_event_type type) {
    WindowGuard guard(view.nectorWindow);
    for (NSTouch* touch in touches) {
        const NSPoint position = touch.normalizedPosition;
        nector_input_event input_event{};
        input_event.type = type;
        input_event.touch_id = static_cast<int64_t>(reinterpret_cast<intptr_t>((__bridge void*)touch.identity));
        input_event.x = static_cast<float>(position.x * view.bounds.size.width);
        input_event.y = static_cast<float>((1.0 - position.y) * view.bounds.size.height);
        input_event.pressure = type == nector_event_touch_end || type == nector_event_touch_cancel ? 0.0f : 1.0f;
        dispatchCocoaEvent(view.nectorWindow, event, input_event);
    }
}

} // namespace

@implementation NectorView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)canBecomeKeyView {
    return YES;
}

- (void)setFrameSize:(NSSize)new_size {
    [super setFrameSize:new_size];
    WindowGuard guard(self.nectorWindow);
    if (self.nectorWindow != nullptr) {
        platformRecordEventStatus(self.nectorWindow,
            platformResizeWindow(self.nectorWindow, static_cast<int32_t>(new_size.width), static_cast<int32_t>(new_size.height)));
    }
}

- (void)drawRect:(NSRect)dirty_rect {
    (void)dirty_rect;
    WindowGuard guard(self.nectorWindow);
    if (self.nectorWindow != nullptr) platformRecordEventStatus(self.nectorWindow, platformRenderWindow(self.nectorWindow));
}

- (void)dispatchKey:(NSEvent*)event down:(BOOL)down {
    nector_input_event input_event{};
    input_event.type = down ? nector_event_key_down : nector_event_key_up;
    input_event.key = cocoaKey(event);
    const bool repeat = down == YES && event.ARepeat == YES;
    if (input_event.key != nector_key_unknown) dispatchCocoaEvent(self.nectorWindow, event, input_event, repeat);
}

- (void)keyDown:(NSEvent*)event {
    WindowGuard guard(self.nectorWindow);
    [self dispatchKey:event down:YES];
    if (self.nectorWindow != nullptr) [self interpretKeyEvents:@[event]];
}

- (void)keyUp:(NSEvent*)event {
    [self dispatchKey:event down:NO];
}

- (void)flagsChanged:(NSEvent*)event {
    const int32_t key = cocoaKey(event);
    if (key != nector_key_shift && key != nector_key_control && key != nector_key_alt) return;
    const NSEventModifierFlags flags = event.modifierFlags;
    const BOOL down = (key == nector_key_shift && (flags & NSEventModifierFlagShift) != 0) ||
                      (key == nector_key_control && (flags & NSEventModifierFlagControl) != 0) ||
                      (key == nector_key_alt && (flags & NSEventModifierFlagOption) != 0);
    [self dispatchKey:event down:down];
}

- (void)insertText:(id)string {
    WindowGuard guard(self.nectorWindow);
    if (self.nectorWindow == nullptr) return;
    NSString* text = [string isKindOfClass:[NSAttributedString class]] ? [string string] : string;
    if (![text isKindOfClass:[NSString class]]) return;
    NSMutableString* filtered = [NSMutableString string];
    for (NSUInteger index = 0; index < text.length; ++index) {
        const unichar character = [text characterAtIndex:index];
        if (character >= 0x20 && character != 0x7f && !(character >= 0xf700 && character <= 0xf8ff)) {
            [filtered appendFormat:@"%C", character];
        }
    }
    const char* utf8 = filtered.UTF8String;
    if (utf8 != nullptr) platformRecordEventStatus(self.nectorWindow,
        platformDispatchText(self.nectorWindow, utf8, std::strlen(utf8)));
}

- (void)doCommandBySelector:(SEL)selector {
    (void)selector;
}

- (void)dispatchMouse:(NSEvent*)event type:(nector_input_event_type)type button:(int32_t)button {
    const NSPoint point = cocoaPoint(self, event);
    nector_input_event input_event{};
    input_event.type = type;
    input_event.button = button;
    input_event.x = static_cast<float>(point.x);
    input_event.y = static_cast<float>(point.y);
    dispatchCocoaEvent(self.nectorWindow, event, input_event);
}

- (void)mouseMoved:(NSEvent*)event { [self dispatchMouse:event type:nector_event_mouse_move button:0]; }
- (void)mouseDragged:(NSEvent*)event { [self dispatchMouse:event type:nector_event_mouse_move button:0]; }
- (void)rightMouseDragged:(NSEvent*)event { [self dispatchMouse:event type:nector_event_mouse_move button:1]; }
- (void)otherMouseDragged:(NSEvent*)event { [self dispatchMouse:event type:nector_event_mouse_move button:2]; }
- (void)mouseDown:(NSEvent*)event { [self dispatchMouse:event type:nector_event_mouse_down button:0]; }
- (void)mouseUp:(NSEvent*)event { [self dispatchMouse:event type:nector_event_mouse_up button:0]; }
- (void)rightMouseDown:(NSEvent*)event { [self dispatchMouse:event type:nector_event_mouse_down button:1]; }
- (void)rightMouseUp:(NSEvent*)event { [self dispatchMouse:event type:nector_event_mouse_up button:1]; }
- (void)otherMouseDown:(NSEvent*)event { [self dispatchMouse:event type:nector_event_mouse_down button:2]; }
- (void)otherMouseUp:(NSEvent*)event { [self dispatchMouse:event type:nector_event_mouse_up button:2]; }

- (void)scrollWheel:(NSEvent*)event {
    const NSPoint point = cocoaPoint(self, event);
    nector_input_event input_event{};
    input_event.type = nector_event_mouse_wheel;
    input_event.x = static_cast<float>(point.x);
    input_event.y = static_cast<float>(point.y);
    if (event.hasPreciseScrollingDeltas) {
        input_event.dx = static_cast<float>(event.scrollingDeltaX);
        input_event.dy = static_cast<float>(-event.scrollingDeltaY);
    } else {
        input_event.dx = static_cast<float>(event.deltaX);
        input_event.dy = static_cast<float>(-event.deltaY);
    }
    dispatchCocoaEvent(self.nectorWindow, event, input_event);
}

- (BOOL)resignFirstResponder {
    WindowGuard guard(self.nectorWindow);
    if (self.nectorWindow != nullptr) {
        nector_input_event event{};
        event.type = nector_event_focus_lost;
        platformRecordEventStatus(self.nectorWindow, platformDispatchInput(self.nectorWindow, event));
    }
    return YES;
}

- (void)touchesBeganWithEvent:(NSEvent*)event {
    dispatchTouch(self, [event touchesMatchingPhase:NSTouchPhaseBegan inView:self], event, nector_event_touch_begin);
}
- (void)touchesMovedWithEvent:(NSEvent*)event {
    dispatchTouch(self, [event touchesMatchingPhase:NSTouchPhaseMoved inView:self], event, nector_event_touch_move);
}
- (void)touchesEndedWithEvent:(NSEvent*)event {
    dispatchTouch(self, [event touchesMatchingPhase:NSTouchPhaseEnded inView:self], event, nector_event_touch_end);
}
- (void)touchesCancelledWithEvent:(NSEvent*)event {
    dispatchTouch(self, [event touchesMatchingPhase:NSTouchPhaseCancelled inView:self], event, nector_event_touch_cancel);
}

@end

@implementation NectorWindowDelegate

- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    if (self.nectorWindow != nullptr) platformCloseWindow(self.nectorWindow);
    return NO;
}

@end

nector_status createPlatformBackend(nector_application_handle application, std::unique_ptr<PlatformBackend>& out_backend) {
    nector_api_try {
        std::unique_ptr<CocoaBackend> backend(new CocoaBackend(application));
        out_backend = std::move(backend);
        return nector_ok;
    } nector_api_catch
}

#endif
