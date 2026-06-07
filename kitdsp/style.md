# Core patterns

Use OO features, but prefer compile-time polymorphism in DSP code (This means lots of templates!)

Most classes should have this structure:

```cpp
class MyProcessor {
    /** don't pass parameters, just pass resources and pass in parameters (incl. sample rate if variable) later */
    MyProcessor(allocatedMemory) { /* ... */; Reset(); }
    /** handling the most common parameters in one big batch fits nicely with polling-based approaches (like hardware/audio-rate modular) */
    void SetParams(a, b, c) { /* ... */ }
    /** Process a single sample */
    float Process(float inputIfNeeded) { return output; }
    /** Reset signal-dependent state */
    void Reset() { /* ... */ }
}
```

parameterization should be achieved with setter methods. If an effect needs a buffer it should accumulate it internally from Process() calls (+provide an overload when efficiency matters).

Don't allocate memory, instead take buffers and any other heap-allocated requirements in the constructor of your class.

buffers should be etl::span. stack allocated member arrays can be whatever but C arrays compile fastest ;)

# Folder Structure
* [apps](./include/kitdsp/apps) Are full end-user applications, often combining multiple blocks together
* [pitch](./include/kitdsp/pitch/) Is pitch detection/shifting.
* [control](./include/kitdsp/control/) Represents control-rate mechanisms. LFOs, envelopes, etc.
* [samplerate](./include/kitdsp/samplerate/) Should be used for resampling, oversampling, multi-rate processing, etc.
* [sampling](./include/kitdsp/sampling/) Covers the core mechanisms for storing samples and playing them back. this includes the delay line!
* [util](./include/kitdsp/util/) Covers the kinds of things any C++ project needs. Unfortunately.
* [volume](./include/kitdsp/volume/) Is for compressors, limiters, etc.

Everything else should be obvious
