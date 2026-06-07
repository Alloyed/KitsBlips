#pragma once

#include "etl/vector.h"
#include "kitdsp/sampling/delayLine.h"
#include "kitdsp/filters/biquad.h"
#include "kitdsp/filters/crossover.h"
#include "kitdsp/samplerate/undersampler.h"
#include "kitdsp/util/spanAllocator.h"

namespace kitdsp {

class AllPassStack {
   public:
    static constexpr size_t kMaxStages = 100;
    static constexpr size_t kMaxSamples = 100;

    static constexpr size_t GetNeededSampleMemory() { return kMaxSamples * kMaxStages; }

    explicit AllPassStack(etl::span<float> mem) {
        assert(mem.size() == GetNeededSampleMemory());
        for (size_t stage = 0; stage < kMaxStages; ++stage) {
            mBuf.emplace_back(etl::span<float>({&mem[stage * kMaxSamples], kMaxSamples}));
        }
    }
    void SetParams(size_t stages, float k, float freq, float sampleRate) {
        this->mNumStages = stages;
        this->k = k;
        this->M = narrow_cast<int32_t>(sampleRate / (freq * 2.0f));
    }

    float Process(float in) {
        float x = in;
        for (size_t stage = 0; stage < mNumStages; ++stage) {
            float vnm = mBuf[stage].Read(M);

            // direct form II
            float vn = x - k * vnm;
            x = k * vn + vnm;

            mBuf[stage].Write(vn);
        }
        return x;
    }

   private:
    float k;
    int32_t M;
    size_t mNumStages;
    etl::vector<DelayLine<float>, kMaxStages> mBuf;
};

class Disperser {
    using Sample = float;

   public:
    static constexpr size_t GetNeededSampleMemory() {
        return
            // mLowAllpass
            AllPassStack::GetNeededSampleMemory() +
            // mHighAllpass
            AllPassStack::GetNeededSampleMemory() +
            // mDelay
            AllPassStack::kMaxSamples +
            // mDelay
            AllPassStack::kMaxSamples;
    }

    explicit Disperser(etl::span<float> mem) : kitdsp::Disperser(SubSpanAllocator<float>(mem)) {}
    void Reset();

    void SetParams(float frequencyHz, float sampleRate, float feedback, size_t numFilters);

    Sample Process(Sample in);
    void ProcessInPlace(etl::span<Sample> in);

   private:
    explicit Disperser(SubSpanAllocator<float> mem)
        : mLowAllpass(mem.alloc(AllPassStack::GetNeededSampleMemory())),
          mHighAllpass(mem.alloc(AllPassStack::GetNeededSampleMemory())),
          mLowDelay(mem.alloc(AllPassStack::kMaxSamples)),
          mHighDelay(mem.alloc(AllPassStack::kMaxSamples)) {}
    Undersampler2x<float> mLowSampler{};
    CrossoverFilter mLowCrossover{};
    AllPassStack mLowAllpass;
    rbj::BiquadFilter mLowFilter{};

    CrossoverFilter mHighCrossover{};
    AllPassStack mHighAllpass;

    DelayLine<float> mLowDelay;
    DelayLine<float> mHighDelay;
};
}  // namespace kitdsp
