custom gui framework! (oh no!)

Tech design notes

- We're piggybacking off of [Dear ImGui](https://github.com/ocornut/imgui/) to get most UI features.
- We're using [Magnum](https://doc.magnum.graphics/) for GPU-native rendering, instead of using a raster library like cairo. OpenGL 3.3 only. gltf is the main import format.
- [SDL3](https://wiki.libsdl.org/) is our windowing on linux, win32 is used directly on windows, and macOS/cocoa... isn't implemented yet.

### Goals

- visually interesting audio plugin UI
- Windows/Mac/Linux support
- Single process/thread, multiple windows

### Non-goals

- SVG/canvas api
- performance
- accessibility (I want to make a custom imgui accesskit wrapper, but not for this lib yet)
- complex/dynamic visual heirarchies and layouts
- full DAW ui (that is to say, piano rolls and such)

### TODO

- Cocoa!!
- Scene position -> screen position and back
- complex responsive layout integration (yoga?)
- DOM+Signals api for a web-like authoring experience
- template language based on pugixml
- Vulkan/other rendering backends?
- Compute shader support as an exposed feature? for doing heavy lifting on the gpu
