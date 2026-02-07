# Processor

In Clapeze, the Processor is the part of your code that will run in the audio thread. For the most part, communication in and out, activation and deactivation, etc. is handled for you in other parts of the code; but just for completeness here's what's actually going on under the hood:

```cpp
/**
 * Called when the plugin is first activated. This is a good place to allocate resources, and do any state setup
 * before the main thread can no longer communicate with the processor directly.
 *
 * [main-thread & !active]
 */
virtual void Activate([[maybe_unused]] double sampleRate, [[maybe_unused]] size_t minBlockSize, [[maybe_unused]] size_t maxBlockSize) {};
/**
 * Called when the plugin is deactivated.
 *
 * [main-thread & active]
 */
virtual void Deactivate() {};
```

are self explanatory. While you can call malloc/new/other complex calls on the audio thread, it's a very very bad idea to. Instead, you should reserve as much as you need in the Activate() call.

```cpp
/**
 * Override to handle discrete events coming from the host.
 *
 * [audio-thread & active & processing]
 */
virtual void ProcessEvent(const clap_event_header_t& event) = 0;
/**
 * Override to process an audio block. the block size can be anything below GetMaxBlockSize(). This is to support
 * sample-accurate timing for events.
 *
 * [audio-thread & active & processing]
 */
virtual void ProcessAudio(const clap_process_t& process, size_t blockStart, size_t blockStop) = 0;
```

Some examples of events are things like note on, note off, parameter changed, and so on. ProcessAudio runs your actual audio in response to those events. Events are sample-accurate. This means that if a event happens, we'll split up the block into two ProcessAudio() calls like so

```cpp
ProcessAudio(process, 0, eventTime);
ProcessEvent(event);
ProcessAudio(process, eventTime, end);
```

This means that in your ProcessAudio() call, you can assume that parameters, note events, etc. are static and have already happened, and in your event call, any internal state will be updated to the point of that sample, providing a simple "chronological" way of thinking (at the expense of bad cache/simd performance)

Here's a full, real-world example of a processor. As you can see, most of the gory details are hidden by other features, and you "simply" query parameters up front and then do your dsp right afterward.

```cpp
class Processor : public EffectProcessor<ParamsFeature::ProcessParameters> {
   public:
    explicit Processor(ParamsFeature::ProcessParameters& params) : EffectProcessor(params) {}
    ~Processor() = default;

    void ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
        float shift = mParams.Get<Params::Shift>();
        float mixf = mParams.Get<Params::Mix>();

        mLeft.SetFrequencyOffset(shift);
        mRight.SetFrequencyOffset(shift);

        for (size_t idx = 0; idx < in.left.size(); ++idx) {
            float left = in.left[idx];
            float processedLeft = mLeft.Process(left);
            out.left[idx] = kitdsp::lerp(left, processedLeft, mixf);
        }
        for (size_t idx = 0; idx < in.right.size(); ++idx) {
            float right = in.right[idx];
            float processedRight = mRight.Process(right);
            out.right[idx] = kitdsp::lerp(right, processedRight, mixf);
        }
    }

    void ProcessReset() override {
        mLeft.Reset();
        mRight.Reset();
    }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        mLeft = kitdsp::FrequencyShifter(mSampleRate);
        mRight = kitdsp::FrequencyShifter(mSampleRate);
    }

   private:
    kitdsp::FrequencyShifter mLeft;
    kitdsp::FrequencyShifter mRight;
};
```
