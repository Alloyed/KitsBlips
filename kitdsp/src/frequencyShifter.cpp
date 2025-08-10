#include "kitdsp/frequencyShifter.h"
#include "kitdsp/lookupTables/sineLut.h"

#define sin_(x) sin2pif_lut(x)
#define cos_(x) sin2pif_lut((x) + 0.25f)

namespace kitdsp {
FrequencyShifter::FrequencyShifter(float sampleRate) : mSampleRate(sampleRate) {
    mHilbert.setSampleRate(sampleRate);
}
void FrequencyShifter::Reset() {
    mHilbert.setHilbertCoefs();
    mPhasor.Reset();
}

float FrequencyShifter::Process(float in) {
    // https://www.dsprelated.com/showarticle/1147.php
    // the real value is i, the "in-phase" component, and the imaginary value is q, the "quadrature" component.
    auto pair = mHilbert.stepPair(in);
    float phase = mPhasor.Process();

    return pair.first * cos_(phase) - pair.second * sin_(phase);
}
}  // namespace kitdsp
