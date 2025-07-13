#ifdef __APPLE__
// not sure this is doable without
// https://github.com/libsdl-org/SDL/issues/12141
/*
extern "C" void *MacInitialise(struct MyPlugin *plugin, uint32_t *bits, uint32_t width, uint32_t height);
extern "C" void MacDestroy(void *mainView);
extern "C" void MacSetParent(void *_mainView, void *_parentView);
extern "C" void MacSetVisible(void *_mainView, bool show);
extern "C" void MacPaint(void *_mainView);

static void GUIPaint(MyPlugin *plugin, bool internal) {
    if (internal) PluginPaint(plugin, plugin->gui->bits);
    MacPaint(plugin->gui->mainView);
}

static void GUICreate(MyPlugin *plugin) {
    assert(!plugin->gui);
    plugin->gui = (GUI *) calloc(1, sizeof(GUI));
    plugin->gui->bits = (uint32_t *) calloc(1, GUI_WIDTH * GUI_HEIGHT * 4);
    PluginPaint(plugin, plugin->gui->bits);
    plugin->gui->mainView = MacInitialise(plugin, plugin->gui->bits, GUI_WIDTH, GUI_HEIGHT);
}

static void GUIDestroy(MyPlugin *plugin) {
    assert(plugin->gui);
    MacDestroy(plugin->gui->mainView);
    free(plugin->gui->bits);
    free(plugin->gui);
    plugin->gui = nullptr;
}

static void GUISetParent(MyPlugin *plugin, const clap_window_t *parent) { MacSetParent(plugin->gui->mainView,
parent->cocoa); } static void GUISetVisible(MyPlugin *plugin, bool visible) { MacSetVisible(plugin->gui->mainView,
visible); } static void GUIOnPOSIXFD(MyPlugin *) {}

extern "C" void MacInputEvent(struct MyPlugin *plugin, int32_t cursorX, int32_t cursorY, int8_t button) {
    if (button == -1) PluginProcessMouseRelease(plugin);
    if (button ==  0) PluginProcessMouseDrag   (plugin, cursorX, GUI_HEIGHT - 1 - cursorY);
    if (button ==  1) PluginProcessMousePress  (plugin, cursorX, GUI_HEIGHT - 1 - cursorY);
    GUIPaint(plugin, true);
}
*/

uint64_t createPlatformTimer([[maybe_unused]] std::function<void()> fn) {
    return 0;
}

void cancelPlatformTimer([[maybe_unused]] uint64_t id) {}

#endif