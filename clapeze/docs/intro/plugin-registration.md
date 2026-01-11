# Plugin Registration

The CLAP format supports multiple plugins from a single binary, so we do too[^src] without extra effort. there are two way to create a clap entry point and register a plugin with it.

### A: Per file

Add an entrypoint.cpp file (name does not matter), and include the following:

```cpp
#include "clapeze/entryPoint.h"

// your program or build system may provide a version of this macro already! if so, use that.
#if defined _WIN32
  #define DLL_PUBLIC __declspec(dllexport)
#else
  #define DLL_PUBLIC __attribute__ ((visibility ("default")))
#endif

extern "C" DLL_PUBLIC const clap_plugin_entry_t clap_entry = CLAPEZE_CREATE_ENTRY_POINT();
```

In the files that define your plugin, you can now register them like so:

```cpp

class Plugin : public clapeze::BasePlugin {/*...*/};
clap_plugin_descriptor_t descriptor {/*...*/};
CLAPEZE_REGISTER_PLUGIN(Plugin, descriptor);
```

This is handy from a modularity standpoint: most of my plugins are a single file, and don't need a header thanks to this.

WARNING: Notably, this trick will fail for projects that use static targets. This is because the linker will look at your .cpp, realize there is no linkage between it and whatever you're connecting your library to, and tree-shake out the registration call. for maximum safety, use approach B.

### B: In the entry point

Add an entrypoint.cpp file (name does not matter), and include the following:

```cpp
#include "clapeze/entryPoint.h"

extern "C" const clap_plugin_entry_t clap_entry = CLAPEZE_CREATE_ENTRY_POINT();

#include "MyPlugin1.h"
CLAPEZE_REGISTER_PLUGIN(MyPlugin::Plugin, MyPlugin::descriptor);
#include "MyPlugin2.h"
CLAPEZE_REGISTER_PLUGIN(MyPlugin::Plugin, MyPlugin::descriptor);
```

You will now also need a header to declare the existence of the plugin.

```cpp
#pragma once
#include "clapeze/basePlugin.h"

namespace MyPlugin{
    class Plugin : public clapeze::BasePlugin {/*...*/};
    clap_plugin_descriptor_t descriptor {/*...*/};
}
```

This works even in static libraries.

[^src]: /clapeze/include/clapeze/entryPoint.h
