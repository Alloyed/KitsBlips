#pragma once

#include <nfd.h>
#include "kitgui.h"

nfdwindowhandle_t NFD_GetWindow(kitgui::WindowRef ref) {
    size_t type{};
    switch (ref.api) {
        case kitgui::WindowApi::Any: {
            type = NFD_WINDOW_HANDLE_TYPE_UNSET;
            break;
        }
        case kitgui::WindowApi::Win32: {
            type = NFD_WINDOW_HANDLE_TYPE_WINDOWS;
            break;
        }
        case kitgui::WindowApi::Cocoa: {
            type = NFD_WINDOW_HANDLE_TYPE_COCOA;
            break;
        }
        case kitgui::WindowApi::X11: {
            type = NFD_WINDOW_HANDLE_TYPE_X11;
            break;
        }
        case kitgui::WindowApi::Wayland: {
            type = NFD_WINDOW_HANDLE_TYPE_UNSET;
            break;
        }
    }
    return {.type = type, .handle = ref.ptr};
}