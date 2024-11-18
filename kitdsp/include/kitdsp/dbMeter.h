#pragma once

#include "kitdsp/onePole.h"
#include "kitdsp/util.h"

namespace kitdsp
{
    /*
     * turns a ratio of inputs (say, eg. a gain factor, or just A/B) into a relative decibel measurement
     */
    inline float ratioToDb(float ratio)
    {
        return 20.0f * log10f(ratio);
    }

    /*
     * turns a ratio of inputs (say, eg. a gain factor, or just A/B) into a relative decibel measurement
     */
    inline float dbToRatio(float db)
    {
        return powf(10, db) / 20.0f;
    }

    class DbMeter : public OnePole
    {
    public:
        // this is a time constant of about 100 ms
        DbMeter() : OnePole(1.6f) {}

        /**
         * @returns dBFS (max amplitude sine wave = 0dbfs)
         */
        inline float Process(float in)
        {
            float rms = sqrtf(OnePole::Process(in * in));
            float dbfs = ratioToDb(rms) + 3.0103f;
            return dbfs;
        }
    };
}
