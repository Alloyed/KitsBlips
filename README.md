# KitsBlips

Monorepo for my Eurorack/synthesizer hobby projects! (that's me, I'm kit)

Everything is always in a state of "not-quite-finished". if something is unclear or unusable in its current state, and you want to use it, feel free to open an issue or contact me.

For technical reasons, this project is ordered by toolset and not by module. this means the full source for a single module may be across multiple folders! Sorry.

unless otherwise listed, all code is MIT licensed. All hardware designs and graphics (kicad, svg etc) are CC-BY-SA 4.0.

## Building

Build orchestration is managed with cmake.

JUCE uses msvc. create a project using:

```
PS E:\code\KitsBlips\build\msvc> cmake ../.. -DENABLE_JUCE=true
```

VCVRack uses mingw. create a project using:

```
kyle@KDESK MINGW64 /e/code/KitsBlips/build/vcvrack
$ cmake ../.. -DENABLE_VCVRACK=true -DRACK_SDK_DIR=/e/code/KitsBlips/sdk/Rack-SDK-2.5.2-win-x64
```

daisy uses gcc and a custom toolchain. to-be-integrated.

Kicad is kicad. to-be-integrated.

## This project structure is cool, how can I use it?

create your own new repo, then

```
    $ git push https://github.com/accountname/new_repo.git +template:main
```

<3
