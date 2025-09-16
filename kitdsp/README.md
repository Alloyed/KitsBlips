# KitDSP

My collection of DSP algorithms and applications. Lots and lots of this code is directly copy-pasted or otherwise ""inspired"" by many sources, but most notably:

- https://github.com/pichenettes/stmlib
- https://github.com/electro-smith/DaisySP/
- KVR audio forums!

Thank you!!!

## Building

KitDSP can be built independently for unit testing purposes.

```bash
$ mkdir build && cd build
$ cmake ..
$ cmake --build .
$ ctest
```

## goals

- modular-first: avoid large buffers/non-real-time effects where possible
- multiplatform: should be safe to use on embedded, VCV rack, and desktop platforms. This implies no allocation, or exceptions, or RTTI.
- C++11: VCV Rack 2 is still stuck on C++11, so that's the lowest standard we need to support. Code should be as modern as that requirement allows.
- Strongly documented: I don't have a good background in DSP theory! So I'll try to explain the basics in ways that I will understand later. no single letter parameters, when I can help it.

## Non-goals

- Production-quality performance: I'm just goofin so whatever runs on my daisy seed is good enough.
- Compile times: lots of templates sorry.
- API Stability: if you'd like to use some of this code, I'd recommend you just copy-paste it into your own DSP library. That's what I did :3
