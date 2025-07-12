custom gui framework! (oh no!)

Tech design notes

- We're piggybacking off of [Dear ImGui](https://github.com/ocornut/imgui/) to get most UI features, and making it "retained" via wrappers. For more traditional UI tasks (preset browser etc.) you're welcome to use ImGui directly!
- We're using [Magnum](https://doc.magnum.graphics/) for GPU-native rendering, instead of using a raster library like cairo. OpenGL 3.3 only. gltf is the main import format.
- [SDL3](https://wiki.libsdl.org/) is our windowing/platform API.

All of these APIs are exposed directly, to avoid too much wrapper boilerplate.

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

- File loading API that will work for physicsfs when when we get there
- DOM wrappers for knob, slider, vslider
- Scene position -> DOM position and back
- complex layout integration (yoga?)
- template language based on pugixml
