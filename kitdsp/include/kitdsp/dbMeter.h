#pragma once

#include "kitdsp/filters/onePole.h"
#include "kitdsp/math/units.h"

namespace kitdsp {
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
