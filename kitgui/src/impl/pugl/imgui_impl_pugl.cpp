#include "imgui.h"
#include "pugl/pugl.h"
#ifndef IMGUI_DISABLE
#include "impl/pugl/imgui_impl_pugl.h"

// https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md#platform-implementing-your-platform-backend

namespace {
ImGuiKey PuglKeyToImGuiKey(uint32_t key);
PuglCursor ImGuiCursorToPuglCursor(uint32_t cursor);
struct PuglBackendData {
    PuglWorld* world;
    PuglView* view;
    double lastTime;
};
}  // namespace

bool ImGui_ImplPugl_InitForOpenGL(PuglWorld* world, PuglView* view) {
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");

    io.BackendPlatformName = "imgui_impl_pugl";
    io.BackendPlatformUserData = IM_NEW(PuglBackendData)(world, view, 0);
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    return true;
}
void ImGui_ImplPugl_Shutdown() {
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData != nullptr && "No platform backend to shutdown, or already shutdown?");
    IM_FREE(io.BackendPlatformUserData);
}
void ImGui_ImplPugl_NewFrame() {
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData != nullptr && "No platform backend");
    auto bd = static_cast<PuglBackendData*>(io.BackendPlatformUserData);
    double now = puglGetTime(bd->world);
    io.DeltaTime = static_cast<float>(now - bd->lastTime);
    bd->lastTime = now;
    auto size = puglGetSizeHint(bd->view, PUGL_CURRENT_SIZE);
    io.DisplaySize = {static_cast<float>(size.width), static_cast<float>(size.height)};
    puglSetCursor(bd->view, ImGuiCursorToPuglCursor(ImGui::GetMouseCursor()));

    // if macOS
    auto scale = puglGetScaleFactor(bd->view);
    io.DisplayFramebufferScale = {static_cast<float>(scale), static_cast<float>(scale)};
}
bool ImGui_ImplPugl_ProcessEvent(const PuglEvent* event) {
    ImGuiIO& io = ImGui::GetIO();
    switch (event->type) {
        // mouse support
        case PUGL_POINTER_IN:
        case PUGL_POINTER_OUT: {
            // for multi-viewport only
            // io.AddMouseViewportEvent();
            return false;
        }
        case PUGL_BUTTON_PRESS:
        case PUGL_BUTTON_RELEASE: {
            auto& e = reinterpret_cast<const PuglButtonEvent&>(*event);
            io.AddMouseButtonEvent(static_cast<int32_t>(e.button), e.type == PUGL_BUTTON_PRESS);
            return true;
        }
        case PUGL_MOTION: {
            auto& e = reinterpret_cast<const PuglMotionEvent&>(*event);
            io.AddMousePosEvent(static_cast<float>(e.x), static_cast<float>(e.y));
            return true;
        }
        case PUGL_SCROLL: {
            auto& e = reinterpret_cast<const PuglScrollEvent&>(*event);
            io.AddMouseWheelEvent(static_cast<float>(e.dx), static_cast<float>(e.dy));
            return true;
        }
        // keyboard support
        case PUGL_KEY_PRESS:
        case PUGL_KEY_RELEASE: {
            auto& e = reinterpret_cast<const PuglKeyEvent&>(*event);
            ImGuiKey key = PuglKeyToImGuiKey(e.keycode);
            io.AddKeyEvent(key, e.type == PUGL_KEY_PRESS);
            return true;
        }
        case PUGL_TEXT: {
            auto& e = reinterpret_cast<const PuglTextEvent&>(*event);
            io.AddInputCharacter(e.keycode);
            return true;
        }

        // other
        case PUGL_FOCUS_IN:
        case PUGL_FOCUS_OUT: {
            io.AddFocusEvent(event->type == PUGL_FOCUS_IN);
            return true;
        }
            // skip
        case PUGL_NOTHING:
        case PUGL_REALIZE:
        case PUGL_UNREALIZE:
        case PUGL_CONFIGURE:
        case PUGL_UPDATE:
        case PUGL_EXPOSE:
        case PUGL_CLOSE:
        case PUGL_CLIENT:
        case PUGL_TIMER:
        case PUGL_LOOP_ENTER:
        case PUGL_LOOP_LEAVE:
        case PUGL_DATA_OFFER:
        case PUGL_DATA:
            break;
    }
    return false;
}

namespace {
ImGuiKey PuglKeyToImGuiKey(uint32_t key) {
    // key is type PuglKey
    switch (key) {
        case PUGL_KEY_NONE:
            return ImGuiKey_None;
        case PUGL_KEY_BACKSPACE:
            return ImGuiKey_Backspace;
        case PUGL_KEY_TAB:
            return ImGuiKey_Tab;
        case PUGL_KEY_ENTER:
            return ImGuiKey_Enter;
        case PUGL_KEY_ESCAPE:
            return ImGuiKey_Escape;
        case PUGL_KEY_DELETE:
            return ImGuiKey_Delete;
        case PUGL_KEY_SPACE:
            return ImGuiKey_Space;
        case PUGL_KEY_PAGE_UP:
            return ImGuiKey_PageUp;
        case PUGL_KEY_PAGE_DOWN:
            return ImGuiKey_PageDown;
        case PUGL_KEY_END:
            return ImGuiKey_End;
        case PUGL_KEY_HOME:
            return ImGuiKey_Home;
        case PUGL_KEY_LEFT:
            return ImGuiKey_LeftArrow;
        case PUGL_KEY_RIGHT:
            return ImGuiKey_RightArrow;
        case PUGL_KEY_DOWN:
            return ImGuiKey_DownArrow;
        case PUGL_KEY_UP:
            return ImGuiKey_UpArrow;
        case PUGL_KEY_PRINT_SCREEN:
            return ImGuiKey_PrintScreen;
        case PUGL_KEY_INSERT:
            return ImGuiKey_Insert;
        case PUGL_KEY_PAUSE:
            return ImGuiKey_Pause;
        case PUGL_KEY_MENU:
            return ImGuiKey_Menu;
        case PUGL_KEY_NUM_LOCK:
            return ImGuiKey_NumLock;
        case PUGL_KEY_SCROLL_LOCK:
            return ImGuiKey_ScrollLock;
        case PUGL_KEY_CAPS_LOCK:
            return ImGuiKey_CapsLock;
        case PUGL_KEY_SHIFT_L:
            return ImGuiKey_LeftShift;
        case PUGL_KEY_SHIFT_R:
            return ImGuiKey_RightShift;
        case PUGL_KEY_CTRL_L:
            return ImGuiKey_LeftCtrl;
        case PUGL_KEY_CTRL_R:
            return ImGuiKey_RightCtrl;
        case PUGL_KEY_ALT_L:
            return ImGuiKey_LeftAlt;
        case PUGL_KEY_ALT_R:
            return ImGuiKey_RightAlt;
        case PUGL_KEY_SUPER_L:
            return ImGuiKey_LeftSuper;
        case PUGL_KEY_SUPER_R:
            return ImGuiKey_RightSuper;
        default:
            break;
    }
    if (key >= '0' && key <= '9') {
        int32_t digit = static_cast<int32_t>(key) - '0';
        return static_cast<ImGuiKey>(ImGuiKey_0 + digit);
    }
    if (key >= 'A' && key <= 'Z') {
        int32_t digit = static_cast<int32_t>(key) - 'A';
        return static_cast<ImGuiKey>(ImGuiKey_A + digit);
    }
    if (key >= PUGL_KEY_F1 && key <= PUGL_KEY_F12) {
        int32_t digit = static_cast<int32_t>(key) - PUGL_KEY_F1;
        return static_cast<ImGuiKey>(ImGuiKey_F1 + digit);
    }
    if (key >= PUGL_KEY_PAD_0 && key <= PUGL_KEY_PAD_9) {
        int32_t digit = static_cast<int32_t>(key) - PUGL_KEY_PAD_0;
        return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + digit);
    }

    return ImGuiKey_None;
}
PuglCursor ImGuiCursorToPuglCursor(uint32_t cursor) {
    switch (cursor) {
        case ImGuiMouseCursor_Arrow:
            return PUGL_CURSOR_ARROW;
        case ImGuiMouseCursor_TextInput:
            return PUGL_CURSOR_CARET;
        case ImGuiMouseCursor_ResizeAll:
            return PUGL_CURSOR_ALL_SCROLL;
        case ImGuiMouseCursor_ResizeNS:
            return PUGL_CURSOR_UP_DOWN;
        case ImGuiMouseCursor_ResizeEW:
            return PUGL_CURSOR_LEFT_RIGHT;
        case ImGuiMouseCursor_ResizeNESW:
            return PUGL_CURSOR_UP_LEFT_DOWN_RIGHT;
        case ImGuiMouseCursor_ResizeNWSE:
            return PUGL_CURSOR_UP_RIGHT_DOWN_LEFT;
        case ImGuiMouseCursor_Hand:
            return PUGL_CURSOR_HAND;
        case ImGuiMouseCursor_NotAllowed:
            return PUGL_CURSOR_NO;
        case ImGuiMouseCursor_Wait:
        case ImGuiMouseCursor_Progress:
        default:
            return PUGL_CURSOR_ARROW;
    }
}
}  // namespace

#endif  // #ifndef IMGUI_DISABLE