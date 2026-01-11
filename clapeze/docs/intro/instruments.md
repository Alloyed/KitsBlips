# Instruments

Simple instruments can use the `clapeze::InstrumentPlugin` base class to provide basic functionality. A full example of usage, with polyphony and parameters, can be found in the examples folder[^src].

## Voices and Voice Allocation

`clapeze::VoicePool`[^voices] is a shortcut to get a host-integrated mechanism for allocating synth voices to notes.

## Polyphonic Expression/Modulation

Is currently unimplemented. I definitely want it, though!

[^src]: /clapeze/include/clapeze/instrumentPlugin.h
[^example]: /clapeze/examples/instrumentExample.cpp
[^voices-src]: /clapeze/include/clapeze/voice.h
