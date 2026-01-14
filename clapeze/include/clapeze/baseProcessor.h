#pragma once

#include <clap/clap.h>
#include <cstddef>
#include <cstdio>
#include "clap/process.h"

namespace clapeze {

enum class ProcessStatus {
    // Processing failed. The output buffer must be discarded.
    Error = CLAP_PROCESS_ERROR,
    // Processing succeeded, keep processing.
    Continue = CLAP_PROCESS_CONTINUE,
    // Processing succeeded, keep processing if the output is not quiet.
    ContinueIfNotQuiet = CLAP_PROCESS_CONTINUE_IF_NOT_QUIET,
    // Rely upon the plugin's tail to determine if the plugin should continue to process.
    // see clap_plugin_tail
    Tail = CLAP_PROCESS_TAIL,
    // Processing succeeded, but no more processing is required,
    // until the next event or variation in audio input.
    Sleep = CLAP_PROCESS_SLEEP,
};

class BasePlugin;
class BaseProcessor {
    friend class BasePlugin;

   public:
    BaseProcessor() {}
    virtual ~BaseProcessor() = default;

    /**
     * Called when the plugin is first activated. This is a good place to allocate resources, and do any state setup
     * before the main thread can no longer communicate with the processor directly.
     *
     * [main-thread & !active]
     */
    virtual void Activate([[maybe_unused]] double sampleRate,
                          [[maybe_unused]] size_t minBlockSize,
                          [[maybe_unused]] size_t maxBlockSize) {};
    /**
     * Called when the plugin is deactivated.
     *
     * [main-thread & active]
     */
    virtual void Deactivate() {};
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
    virtual ProcessStatus ProcessAudio(const clap_process_t& process, size_t blockStart, size_t blockStop) = 0;
    /**
     * Override to communicate with the main thread. Called once per process.
     *
     * [audio-thread & active & processing]
     */
    virtual void ProcessFlush(const clap_process_t& process) = 0;
    /**
     * Override to reset internal state. you should kill all playing voices, clear all buffers, reset oscillator phases,
     * etc.
     *
     * [audio-thread & active]
     */
    virtual void ProcessReset() {};

    /* only valid while processing. using contextual state. */
    void SendEvent(clap_event_header_t& event) {
        event.time += mTime;  // this is probably very not smart
        mOutEvents->try_push(mOutEvents, &event);
    }

   protected:
    /**
     * Returns the sample rate in hz
     */
    double GetSampleRate() const { return mSampleRate; }
    size_t GetMaxBlockSize() const { return mMinBlockSize; }
    size_t GetMinBlockSize() const { return mMaxBlockSize; }

    // returns a ratio of cpu time spent vs audio time generated. above 1, we've failed to meet our deadline.
    double mLastTimeSpentRatio{};

   private:
    double mSampleRate{};
    size_t mMinBlockSize{};
    size_t mMaxBlockSize{};

    const clap_output_events_t* mOutEvents{};
    uint32_t mTime{};
};
}  // namespace clapeze