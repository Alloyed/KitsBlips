#pragma once

#include "kitdsp/filters/onePole.h"
#include "kitdsp/math/util.h"

namespace kitdsp {
/*
 * turns a ratio of inputs (say, eg. a gain factor, or just A/B) into a relative
 * decibel (dbFS) measurement
 */
inline float ratioToDb(float ratio) {
    return 20.0f * log10f(ratio);
}

/*
 * turns a dbfs measurement into a gain factor
 */
inline float dbToRatio(float db) {
    return powf(10, db / 20.0f);
}

class DbMeter : public OnePole {
   public:
    DbMeter(float sampleRate) : OnePole() {
        // this is a time constant of about 100 ms
        SetFrequency(1.6f, sampleRate);
    }

    /**
     * @returns dBFS (max amplitude sine wave = 0dbfs)
     */
    inline float Process(float in) {
        float rms = sqrtf(OnePole::Process(in * in));
        float dbfs = ratioToDb(rms) + 3.0103f;
        return dbfs;
    }
};
}  // namespace kitdsp
