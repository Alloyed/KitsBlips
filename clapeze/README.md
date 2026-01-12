# Clapeze

Clapeze (rhymes with Trapeze) is a framework for writing DAW audio plugins. It builds directly to the CLAP api, and handles common requirements like audio <-> main thread communication and parameter management. it _does not_ handle GUI support, but it provides hooks for you to bring your own.

## Dependencies

Clapeze uses

- C++20 features.
- The [CLAP](https://github.com/free-audio/clap) api (as its plugin format)
- The [ETL Embedded Template Library](https://github.com/ETLCPP/etl) For datastructures that can be safely used on the audio thread
- [{fmt}](https://github.com/fmtlib/fmt) For string formatting
- [miniz](https://github.com/richgel999/miniz) For asset loading

All are compiled statically into clapeze. It should be portable to all platforms clap itself can be hosted. Thanks, those projects!

## Alternatives

clapeze is not really intended to be used as a third party library. There are no stability guarantees and no release schedule. This library is for me, and I want the freedom to explore "bad" ideas. As an alternative, consider:

- [JUCE](https://juce.com/)
  - JUCE will work for you, probably! This is the conservative and smart choice.
- [CPLUG](https://github.com/Tremus/CPLUG)
  - Has a focus on minimal-ness that feels in-line with the philosophy of this project.
- Just write to [CLAP](https://github.com/free-audio/clap) directly!
  - The api itself is not that bad, and if you'd prefer C++ syntax you can look at [clap-helpers](https://github.com/free-audio/clap-helpers)

That said, I encourage you to take inspiration or copy-paste segments of this library. Lots of comments and explanations will try to cover the _design philosophy_ and architectural decisions clapeze is making, over any crusty specifics.

## Building

clapeze is a normal cmake project. It has examples, but no automated tests yet.

```bash
$ mkdir build && cd build
$ cmake ..
$ cmake --build .
```

## Documentation

Working examples exist in the [examples/](examples) folder.

Core concepts, and basic explanations for how to do things can be found in the [docs/intro](docs/intro) folder.

Insight into the design decisions made by clapeze can be found under the [docs/thoughts](docs/thoughts) folder.
