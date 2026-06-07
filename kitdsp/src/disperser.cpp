#include "kitdsp/apps/disperser.h"
#include "kitdsp/lookupTables/sineLut.h"
#include "kitdsp/util/macros.h"
#include "kitdsp/math/interpolate.h"
#include "kitdsp/math/util.h"

namespace kitdsp {

void Disperser::Reset() {}

void Disperser::SetParams(float frequencyHz, float sampleRate, float feedback, size_t numFilters) {}

Disperser::Sample Disperser::Process(Sample startingInput) {
    float clf = mLowSampler.Process(startingInput, [&](float in) {
        float high{}, low{};
        mLowCrossover.Process(in, high, low);

        high = mLowAllpass.Process(high);

        // int32_t groupDelay = k * M * ((1 - a * a) / (1 + 2 * a * cos2pif_lut(freq * k) + a * a));
        int32_t groupDelay = 0;
        mLowDelay.Write(low);
        low = mLowDelay.Read(groupDelay);

        return high + low;
    });

    float chf{};
    {
        float high{}, low{};
        mHighCrossover.Process(startingInput, high, low);

        low = mHighAllpass.Process(low);

        // int32_t groupDelay = k * M * ((1 - a * a) / (1 + 2 * a * cos2pif_lut(freq * k) + a * a));
        int32_t groupDelay = 0;
        mHighDelay.Write(high);
        high = mHighDelay.Read(groupDelay);
        chf = high + low;
    }

    float mix = 1.0f;
    return kitdsp::lerp(startingInput, clf + chf, mix);
}

void Disperser::ProcessInPlace(etl::span<Sample> startingInput) {}
}  // namespace kitdsp