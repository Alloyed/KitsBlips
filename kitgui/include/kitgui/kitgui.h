#pragma once

namespace kitgui {
void init();
void deinit();
namespace platform {
enum class Api { Any, Win32, Cocoa, X11, Wayland };
}
}  // namespace kitgui