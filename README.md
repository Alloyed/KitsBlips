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

once you have all your configurations set up, you can quickly rebuild all/run tests using `rebuild-all.sh`.

## Projects

For Hardware:

- [/daisy](daisy/README.md)
  - DSP code for hardware modules, using the Daisy microcontroller.
- [/kicad](kicad/README.md)
  - Hardware design files

For VCV Rack:

- [/vcvrack](daisy/README.md)
  - Modules for VCV Rack

For DAWS:

- [/daw](clap/README.md)
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

## AI statement

KitsBlips will not accept PRs that are generated wholly or in significant part by generative AI. Future development will not use Generative AI.

There are a few older commits where I tried it for automatic refactoring, found it less than successful, and cleaned it up afterward. You can try the same thing; I wouldn't be able to tell the difference probably, but my conclusion to that experiment was that it wasn't worth it at the time.
