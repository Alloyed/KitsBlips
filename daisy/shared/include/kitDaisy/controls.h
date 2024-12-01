#pragma once
#include <daisy.h>
#include <kitdsp/math/util.h>

namespace kitDaisy {
namespace controls {

inline float GetKnob(daisy::AnalogControl& knob) {
    return kitdsp::clamp(knob.Value(), 0.0f, 1.0f);
}

inline float GetKnob(daisy::AnalogControl* knob) {
    return knob ? kitdsp::clamp(knob->Value(), 0.0f, 1.0f) : 0.0f;
}

inline float GetJack(daisy::AnalogControl& jack) {
    return kitdsp::clamp(jack.Value(), -1.0f, 1.0f);
}

inline float GetJack(daisy::AnalogControl* jack) {
    return jack ? kitdsp::clamp(jack->Value(), -1.0f, 1.0f) : 0.0f;
}

class LinearControl {
    public:
        LinearControl(daisy::AnalogControl& knob, daisy::AnalogControl* jack, float min = 0.0f, float max = 1.0f):
            mKnob(knob),
            mJack(jack),
            mMin(min),
            mMax(max)
        {
        }

        float Get() {
            float knob = GetKnob(mKnob);
            float jack = GetJack(mJack);
            float joined = kitdsp::clamp(knob + jack, 0.0f, 1.0f);

            return kitdsp::lerpf(mMin, mMax, joined);
        }
    private:

        daisy::AnalogControl& mKnob;
        daisy::AnalogControl* mJack;
        float mMin;
        float mMax;
};

class AttenuvertedControl {
    public:
        AttenuvertedControl(daisy::AnalogControl* knob, daisy::AnalogControl& attenuverter, daisy::AnalogControl& jack, float min, float max):
            mKnob(knob),
            mAttenuverter(attenuverter),
            mJack(jack),
            mMin(min),
            mMax(max)
        {
        }

        float Get() {
            float knob = GetKnob(mKnob);
            float jack = GetJack(mJack);
            float attenuvert = kitdsp::lerpf(-1.0f, 1.0f, GetKnob());
            float joined = kitdsp::clamp(knob + (jack * attenuvert), 0.0f, 1.0f);

            return kitdsp::lerpf(mMin, mMax, joined);
        }
    private:

        daisy::AnalogControl* mKnob;
        daisy::AnalogControl& mAttenuverter;
        daisy::AnalogControl& mJack;
        float mMin;
        float mMax;
};
}
}  // namespace kitDaisy