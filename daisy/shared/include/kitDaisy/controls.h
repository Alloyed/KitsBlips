#pragma once
#include <daisy.h>
#include <daisy_patch_sm.h>
#include <kitdsp/math/units.h>
#include <kitdsp/math/util.h>

namespace kitDaisy {
namespace controls {

inline float GetKnobN(daisy::AnalogControl& knob) {
    return kitdsp::clamp(knob.Value(), 0.0f, 1.0f);
}

inline float GetKnobN(daisy::AnalogControl* knob) {
    return knob ? kitdsp::clamp(knob->Value(), 0.0f, 1.0f) : 0.0f;
}

inline float GetJackN(daisy::AnalogControl& jack) {
    return kitdsp::clamp(jack.Value(), -1.0f, 1.0f);
}

inline float GetJackN(daisy::AnalogControl* jack) {
    return jack ? kitdsp::clamp(jack->Value(), -1.0f, 1.0f) : 0.0f;
}

class ControlCalibrator {
   public:
    ControlCalibrator(daisy::QSPIHandle& qspi) : mStorage(qspi) {}

    void Init() { mStorage.Init({}, 0); }

    void GetPinCalibration(int32_t pin, float& scale, float& offset) {
        const auto& values = mStorage.GetSettings().adcValues[pin];
        offset = values.offset;
        scale = values.scale;
    }

    void ApplyControlCalibration(daisy::patch_sm::DaisyPatchSM& hw) {
        for (int32_t pin = daisy::patch_sm::CV_1; pin < daisy::patch_sm::ADC_LAST; ++pin) {
            const auto& values = mStorage.GetSettings().adcValues[pin];
            hw.controls[pin].SetOffset(values.offset);
            hw.controls[pin].SetScale(values.scale);
        }
    }

    void StoreKnobCalibration(int32_t pin, float zeroVolt, float fiveVolt) {
        auto& values = mStorage.GetSettings().adcValues[pin];
        // rescaled to voltage equivalents
        values.scale = 5.0f / (fiveVolt - zeroVolt);
        values.offset = -zeroVolt;
    }

    void StorePinCalibration(int32_t pin, float oneVolt, float threeVolt) {
        auto& values = mStorage.GetSettings().adcValues[pin];
        // rescaled to voltage equivalents
        values.scale = 2.0f / (threeVolt - oneVolt);
        values.offset = 1.0f - (oneVolt * values.scale);
    }

    void Save() { mStorage.Save(); }

   private:
    struct AdcCalibrationSetting {
        float scale = 1.0f;
        float offset = 0.0f;
    };
    struct KitCalibrationSettings {
        AdcCalibrationSetting adcValues[daisy::patch_sm::ADC_LAST];

        bool operator==(KitCalibrationSettings const& right) const {
            return std::memcmp(this, &right, sizeof(*this)) == 0;
        };
        bool operator!=(KitCalibrationSettings const& right) const {
            return std::memcmp(this, &right, sizeof(*this)) != 0;
        };
    };
    using Storage = daisy::PersistentStorage<KitCalibrationSettings>;
    Storage mStorage;
};

class LinearControl {
   public:
    LinearControl(daisy::AnalogControl& knob, daisy::AnalogControl* jack, float min = 0.0f, float max = 1.0f)
        : mKnob(knob), mJack(jack), mMin(min), mMax(max) {}

    float Get() {
        float knob = GetKnobN(mKnob);
        float jack = GetJackN(mJack);
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
    AttenuvertedControl(daisy::AnalogControl* knob,
                        daisy::AnalogControl& attenuverter,
                        daisy::AnalogControl& jack,
                        float min,
                        float max)
        : mKnob(knob), mAttenuverter(attenuverter), mJack(jack), mMin(min), mMax(max) {}

    float Get() {
        float knob = GetKnobN(mKnob);
        float jack = GetJackN(mJack);
        float attenuvert = kitdsp::lerpf(-1.0f, 1.0f, GetKnobN(mAttenuverter));
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

// It's assumed that this is used in a module that's using ControlCalibrator, above
class FrequencyControl {
   public:
    FrequencyControl(daisy::AnalogControl& knob, daisy::AnalogControl& jack) : mKnob(knob), mJack(jack) {}

    float Get() {
        // coarse
        float knob = mKnob.Value() * 69.0f;
        // 1v/oct
        float jack = mJack.Value() * 12.0f;

        return kitdsp::midiToFrequency(knob + jack);
    }

    daisy::AnalogControl& mKnob;
    daisy::AnalogControl& mJack;
    float mJackScale;
    float mJackOffset;
};
}  // namespace controls
}  // namespace kitDaisy