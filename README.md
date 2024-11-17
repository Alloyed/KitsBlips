# KitsBlips

Monorepo for my Eurorack/synthesizer hobby projects! (that's me, I'm kit)

Everything is always in a state of "not-quite-finished". if something is unclear or unusable in its current state, and you want to use it, feel free to open an issue or contact me.

For technical reasons, this project is ordered by toolset and not by module. this means the full source for a single module may be across multiple folders! Sorry.

unless otherwise listed, all code is MIT licensed. All hardware designs and graphics (kicad, svg etc) are CC-BY-SA 4.0.

## Building

### Daisy
you can skip specifying compiler if your intended compiler is part of your `$PATH`.

```bash
$ mkdir -p daisy/build && cd daisy/build
$ cmake .. -DCMAKE_C_COMPILER="<path to arm-non-eabi-gcc>" -DCMAKE_BUILD_TYPE="MinSizeRel"
$ cmake --build .
```

scripts for convenient flashing todo

### VCV Rack
Building on windows requires mingw-64, see [official docs](https://vcvrack.com/manual/Building#Windows) for details. you will need to run cmake from a mingw shell.

```bash
$ mkdir -p vcvrack/build && cd vcvrack/build
$ cmake ..
$ cmake --build .

$ cmake --install . --prefix dist
$ cp -r dist/* ~/.local/share/Rack2/plugins/lin-x64/
# or
$ cp -r dist/* $LOCALAPPDATA/Rack2/plugins/win-x64/
```

### JUCE
This folder is aggressively underutilized right now, but it's there!

```bash
$ mkdir -p juce/build && cd juce/build
$ cmake ..
$ cmake --build .
```

### KitDSP
KitDSP can be used independently for unit testing purposes.

```bash
$ mkdir -p kitdsp/build && cd kitdsp/build
$ cmake ..
$ cmake --build .

$ ctest
```


## This project structure is cool, how can I use it?

!!!This template needs to be updated!!! ping me first, and I can update it for you.

create your own new repo, then

```
    $ git push https://github.com/accountname/new_repo.git +template:main
```

<3
