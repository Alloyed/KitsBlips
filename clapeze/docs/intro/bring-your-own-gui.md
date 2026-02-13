# Bring your own GUI

Clapeze doesn't come with any gui framework by default. Instead, you can subclass `clapeze::BaseGuiFeature`. This is a thin wrapper around the clap `EXT_GUI` extension.

```c++
#include <clapeze/features/baseGuiFeature.h>

class MyGuiFeature : public clapeze::BaseGuiFeature {
    bool IsApiSupported(ClapWindowApi api, bool isFloating) override;
    bool GetPreferredApi(ClapWindowApi& apiOut, bool& isFloatingOut) override;
    bool Create(ClapWindowApi api, bool isFloating) override;
    void Destroy() override;
    bool SetScale(double scale) override;
    bool GetSize(uint32_t& widthOut, uint32_t& heightOut) override;
    bool CanResize() override;
    bool GetResizeHints(clap_gui_resize_hints_t& hintsOut) override;
    bool AdjustSize(uint32_t& widthInOut, uint32_t& heightInOut) override;
    bool SetSize(uint32_t width, uint32_t height) override;
    bool SetParent(WindowHandle handle) override;
    bool SetTransient(WindowHandle handle) override;
    void SuggestTitle(std::string_view title) override;
    bool Show() override;
    bool Hide() override;
}
```

And then in your plugin:

```c++
void MyPlugin::Config()  {
    ...
    ConfigFeature<MyGuiFeature>(GetHost());
    ...
}
```

# How to implement

the clap gui extension is not especially easy to implement, so let's discuss the requirements quickly.

---

### M Processes, N Windows

The user might open up multiple instances of your plugin at the same time, so you should be robust to that. They may be sharing a "main" thread, so blocking event loops are not an option. Some hosts will seperate plugins (including those by third parties) by process, others won't, so if your framework has process-shared state be careful not to pollute it.

### The Windowing Framework is the interface

Clap doesn't define an event loop or surface system, instead, you get a `clap_window_t`, which is a native handle to the parent window. You're expected to figure out your own event loop. You're also expected to figure out highdpi support (though the host _might_ provide a desired zoom level).

### Embed vs Floating

Almost every host and windowing framework expects to "embed" a window into the parent handle it provides you. this means that the plugin host actually creates the stuff you'd normally associate with a window, the decorations around it, and often little widgets on the inside for managing it.

Linux/Wayland doesn't have any kind of equivalent to the concept of embedding, so the API's surface area doubles to support "floating" windows. These are fully separate windows with a looser connection to the parent window; this is what happen when you spawn a dialog or file picker, for example. This is currently supported in very few hosts, so I might hide it from Clapeze's API. To be continued!

### Create/Destroy are common

Many hosts will create windows when the user focuses them, and then destroy those windows when the user changes focus in the parent app. this means you'll be calling create and delete multiple times in a given session, and it makes sense to optimize these as much as possible. In my own plugins I will create gui resources on the first call to create, but only delete them when the entire plugin is deleted; this is to avoid expensive recreate calls in this loop.

---

With these requirements added up, you may find it easier to write separate, per-platform window backends that ensure these rules are met, as opposed to using a cross-platform windowing api like SDL.
