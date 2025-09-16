# KitsBlips

Monorepo for my Eurorack/synthesizer hobby projects! (that's me, I'm kit)

Everything is always in a state of "not-quite-finished". if something is unclear or unusable in its current state, and you want to use it, feel free to open an issue or contact me.

For technical reasons, this project is ordered by toolset and not by module. this means the full source for a single module may be across multiple folders! Sorry.

unless otherwise listed, all code is MIT licensed. All hardware designs and graphics (kicad, svg etc) are CC-BY-SA 4.0.

## Building/Installing

Each project/folder has a specific readme that includes extra steps if necessary, but in general, builds are managed using cmake.

```
$ mkdir -p <folder>/build && cd <folder>/build
$ cmake .. -DCMAKE_BUILD_TYPE=<build type: Debug, MinSizeRel, RelWithDebInfo>
$ cmake --build . --config=<the build type>
$ cmake --install . --prefix <location> # if the target supports installation
$ ctest # if the target has unit tests
```

## Projects

For Hardware:

- [/daisy](daisy/README.md)
  - DSP code for hardware modules
- [/kicad](kicad/README.md)
  - Hardware design files

For VCV Rack:

- [/vcvrack](daisy/README.md)
  - Modules for VCV Rack

For DAWS:

- [/clap](clap/README.md)
  - Modules for DAWS (VST3/CLAP plugins)
- [/clapeze](clapeze/README.md)
  - Microframework for writing clap plugins
- [/kitgui](kitgui/README.md)
  - UI library for clap plugins

Shared:

- [/panels](panels/README.md)
  - Art/Panel designs.
- [/kitdsp](kitdsp/README.md)
  - Shared DSP code, runs on all platforms
- [/kitdsp-gpl](kitdsp-gpl/README.md)
  - DSP code with GPL algorithms. This is the only folder that comes with extra license restrictions!
