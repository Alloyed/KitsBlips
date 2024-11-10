# snecho

A high-level emulation of the echo/reverb effects featured in the SNES/PSX audio chips. These effects are "fixed-function", meaning every game released for these platforms had access to the same set of effects.

Both effects run at their original sample rates and 16-bit depth, resulting in a "crunchy" sound.

## Author

<!-- Insert Your Name Here -->

## Description

Currently formatted for patch.init().

Two modes are supported. when the LED is off, we're in SNES mode. when the LED is on, we're in PSX mode.

In SNES mode:

- CV 1 + CV 5 - Echo delay (16 ms increments, only changes on buffer "wraparound")
- CV 2 + CV 6 - Echo feedback
- CV 3 + CV 7 - Feedback filter
- CV ?? + CV ?? - Echo smooth delay mod. not mapped atm

- DAC out 1 - mixed output
- DAC out 2 - DAC 1, inverted. On SNES hardware this was used optionally as the right channel for a cheap stereo "widening" effect

In PSX mode:

- no parameters yet!

- DAC out 1 - mixed output
- DAC out 2 - mixed output duplicated. The PSX echo is a mono effect.

In Both:

- CV 4 + CV 8 - Wet/Dry
- BTN 7 or gate 1 - (trigger) clear delay buffers
- BTN 8 xor gate 2 - toggle between snes and psx mode
