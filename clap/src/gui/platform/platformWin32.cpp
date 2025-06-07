#ifdef _WIN32

namespace {
void getPlatformHandles(SDL_Window* sdlWindow, HWND& hWindow) {
    SDL_PropertiesID windowProps = SDL_GetWindowProperties(sdlWindow);
    hWindow = static_cast<HWND>(SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, 0));
}
}  // namespace

namespace platformGui {
void onCreateWindow(SDL_Window* sdlWindow) {
    // do nothing
    (void)sdlWindow;
}

bool setParent(SDL_Window* sdlWindow, const WindowHandle& parent) {
    HWND childWindow;
    getPlatformHandles(sdlWindow, childWindow);
    HWND parentWindow = static_cast<HWND>(parent.ptr);

    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setparent?redirectedfrom=MSDN#remarks
    // per documentation, we need to manually update the style to reflect child status
    const DWORD style = GetWindowLong(child_data->hwnd, GWL_STYLE);
    SetWindowLong(child_data->hwnd, GWL_STYLE, (style | WS_CHILD) ^ WS_POPUP);
    SetParent(childWindow, parentWindow);

    return true;
}

bool setTransient(SDL_Window* sdlWindow, const WindowHandle& parent) {
    // same implementation as setParent()
    setParent(sdlWindow, parent);

    return true;
}
}  // namespace platformGui
#endif