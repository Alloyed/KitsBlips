#pragma once

namespace kitgui {
enum class WindowApi { Any, Win32, Cocoa, X11, Wayland };
struct WindowRef {
    WindowApi api;
    void* ptr;
};
inline WindowRef wrapWindow(WindowApi api, void* windowPtr) {
    return {api, windowPtr};
}
}  // namespace kitgui