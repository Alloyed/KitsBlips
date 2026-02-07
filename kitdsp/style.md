# Core patterns

Use OO features, but prefer compile-time polymorphism in DSP code (This means lots of templates!)

Most classes should have this structure:

```cpp
/** Process a single sample */
float Process(float inputIfNeeded) { return output; }
/** Reset signal-dependent state */
void Reset();
```

parameterization should be achieved with setter methods. If an effect needs a buffer it should accumulate it internally from Process() calls (+provide an overload when efficiency matters).

Don't allocate memory, instead take buffers and any other heap-allocated requirements in the constructor of your class.

buffers should be etl::span. stack allocated member arrays can be whatever but C arrays compile fastest ;)
