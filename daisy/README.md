Hi!
These are selected algorithms designed for the Patch.init() UI.

It may make sense to build front panels for some of these, but generally speaking I want to use my Patch.init() as a reusable, easy-to-reprogram module, so lite(tm) versions of these algorithms will always exist around it as my base.

If something doesn't have a readme, check the source code! I usually track UI state in a comment at the very top. Certain parameters may only be accessible by source code edits.

TODO:

- when libdaisy cmake is merged, this will be merged with https://github.com/Alloyed/KitsBlips
- I really want to make a patch.init() variant with a screen and encoder.
- track algorithm quality/stability status better

# Building

you can skip specifying compiler if your intended compiler is part of your `$PATH`.

```bash
$ mkdir -p daisy/build && cd daisy/build
$ cmake .. -DCMAKE_C_COMPILER="<path to arm-non-eabi-gcc>" -DCMAKE_BUILD_TYPE="MinSizeRel"
$ cmake --build .
```

scripts for convenient flashing todo
