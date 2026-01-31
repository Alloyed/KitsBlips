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

### GLTF

We use gltf as our scene presentation format. your file should include

- a camera
- any lighting you want

in addition to the subjects of your scene. we support

- png/jpeg/webp textures
- PBR materials + lights

we don't support:

- any other kind of texture
- mesh compression
- transparent materials

but we'd like to! someday. To make your file manipulatable, export your scene with node names included. You can query based off of them.

### TODO

- Cocoa!!
- Scene position -> screen position and back
- complex responsive layout integration (yoga?)
- DOM+Signals api for a web-like authoring experience
- template language based on pugixml
- Vulkan/other rendering backends?
- Compute shader support as an exposed feature? for doing heavy lifting on the gpu
