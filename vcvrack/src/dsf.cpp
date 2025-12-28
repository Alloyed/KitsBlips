#include "kitdsp/math/util.h"
#include "kitdsp/osc/dsfOscillator.h"
#include "plugin.hpp"
#include "SvgHelper.hpp"

using namespace kitdsp;
using float4 = simd::float_4;
using int4 = simd::int32_4;

float4 approxTanhf(float4 x) {
    float4 xx = x * x;
    float4 x1 = x + (0.16489087f + 0.00985468f * xx) * (x * xx);
    return x1 * simd::rsqrt(1.0f + x1 * x1);
}

struct NKK2 : app::SvgSwitch {
    NKK2() {
        shadow->opacity = 0.0;
        // addFrame(Svg::load(asset::system("res/ComponentLibrary/NKK_1.svg")));
        addFrame(Svg::load(asset::system("res/ComponentLibrary/NKK_2.svg")));
        addFrame(Svg::load(asset::system("res/ComponentLibrary/NKK_0.svg")));
    }
};

struct Davies1900hForestKnob : Davies1900hKnob {
	Davies1900hForestKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance,"res/Davies1900hForest.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance,"res/Davies1900hForest_bg.svg")));
	}
};

struct Davies1900hChocoKnob : Davies1900hKnob {
	Davies1900hChocoKnob() {
		setSvg(Svg::load(asset::plugin(pluginInstance,"res/Davies1900hChoco.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance,"res/Davies1900hChoco_bg.svg")));
	}
};

// we need one secret -1 and +1 ratio for the fine tune knob
static constexpr float cModulatorRatios[] = {0.0f, 0.25f, 0.5f, 1.f, 2.f, 3.f, 4.f, 5.f, 10.0f};
static constexpr float cNumRatios = sizeof(cModulatorRatios) - 2.0f;
static constexpr float cMaxLevel = 0.9f;

float4 lookupRatio(int4 ratioIndex) {
    float4 out;
    out[0] = cModulatorRatios[ratioIndex[0]];
    out[1] = cModulatorRatios[ratioIndex[1]];
    out[2] = cModulatorRatios[ratioIndex[2]];
    out[3] = cModulatorRatios[ratioIndex[3]];
    return out;
}

struct Dsf : Module {
    enum ParamId {
        COARSE_PARAM,
        FINE_PARAM,
        FM_PARAM,
        MOD_MODE_PARAM,
        MOD_COARSE_PARAM,
        MOD_FINE_PARAM,
        MOD_FM_PARAM,
        LEVEL_PARAM,
        LEVEL_CV_PARAM,
        PARAMS_LEN
    };
    enum InputId { PITCH_INPUT, FM_INPUT, MOD_PITCH_INPUT, MOD_FM_INPUT, LEVEL_INPUT, INPUTS_LEN };
    enum OutputId { OUT_MAIN_OUTPUT, OUT_AUX_OUTPUT, OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    Dsf() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(COARSE_PARAM, -2.f, 4.f, 0.f, "Octave");
        configParam(FINE_PARAM, -7.f, 7.f, 0.f, "Fine (+/- 7 semitones)");
        configParam(FM_PARAM, -1.f, 1.f, 0.f, "FM (linear)");
        configSwitch(MOD_MODE_PARAM, 0.f, 1.f, 0.f, "Modulator Pitch tracking", {"Ratio", "Fixed"});
        configParam(MOD_COARSE_PARAM, -2.f, 4.f, 0.f, "Coarse");
        configParam(MOD_FINE_PARAM, -7.f, 7.f, 0.f, "Fine");
        configParam(MOD_FM_PARAM, -1.f, 1.f, 0.f, "FM (linear)");
        configParam(LEVEL_PARAM, 0.f, 1.0f, 0.5f, "Harmonics Level");
        configParam(LEVEL_CV_PARAM, -1.f, 1.f, 0.f, "Harmonics CV scaling");
        configInput(PITCH_INPUT, "Pitch");
        configInput(MOD_PITCH_INPUT, "Harmonics");
        configInput(FM_INPUT, "FM");
        configInput(MOD_FM_INPUT, "Harmonics FM");
        configInput(LEVEL_INPUT, "Harmonics Level");
        configOutput(OUT_MAIN_OUTPUT, "Main out (DSF algorithm 1)");
        configOutput(OUT_AUX_OUTPUT, "Aux out (DSF algorithm 3)");
    }

    void onReset(const ResetEvent& e) override {
        Module::onReset(e);
        for (auto& osc : oscs) {
            osc.Reset();
        }
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override {
        Module::onSampleRateChange(e);
        for (auto& osc : oscs) {
            osc.Init(e.sampleRate);
        }
    }

    void process(const ProcessArgs& args) override {
        if (!outputs[OUT_MAIN_OUTPUT].isConnected() && !outputs[OUT_AUX_OUTPUT].isConnected()) {
            // nothing to compute!
            return;
        }

        // shared parameters:
        float octave = floor(params[COARSE_PARAM].getValue() + 0.5f);
        bool useRatioModulator = params[MOD_MODE_PARAM].getValue() == 0.0f;

        // ratio mod params
        float ratioFine = params[MOD_FINE_PARAM].getValue() / 14.0f;

        // fixed mod params
        float modOctave = floor(params[MOD_COARSE_PARAM].getValue() + 0.5f);
        float modFine = (params[MOD_FINE_PARAM].getValue() / 12.0f);

        // Get desired number of channels from a "primary" input.
        // If this input is unpatched, getChannels() returns 0, but we should
        // still generate 1 channel of output.
        int channels = max(1, inputs[PITCH_INPUT].getChannels());

        if (channels != mLastChannels) {
            mLastChannels = channels;
            // reset oscs, their phases may be out of sync
            for (auto& osc : oscs) {
                osc.Reset();
            }
        }

        // Iterate through each active channel.
        for (int c = 0; c < channels; c += 4) {
            int32_t channelsInBlock = std::min(channels - c, 4);

            float4 freqCarrier = 0.0f;
            {
                float4 pitch = octave + inputs[PITCH_INPUT].getPolyVoltageSimd<float4>(c) +
                               (params[FINE_PARAM].getValue() / 12.0f);
                float4 fm = params[FM_PARAM].getValue() * inputs[FM_INPUT].getPolyVoltageSimd<float4>(c);
                freqCarrier = dsp::FREQ_C4 * (dsp::exp2_taylor5(pitch) + fm);
            }

            float4 freqModulator = 0.0f;
            if (useRatioModulator) {
                // ratio
                float4 ratioCoarse = simd::clamp(
                    params[MOD_COARSE_PARAM].getValue() + 2.0f + inputs[MOD_PITCH_INPUT].getPolyVoltageSimd<float4>(c),
                    0.0f, 6.0f);
                int4 ratioIndex = simd::trunc(ratioCoarse + 0.5f) + 1;
                float4 ratio;
                if (ratioFine < 0.0f) {
                    float4 ratio0 = lookupRatio(ratioIndex - 1);
                    float4 ratio1 = lookupRatio(ratioIndex);
                    ratio = lerpf<float4>(ratio0, ratio1, simd::fabs(ratioFine));
                } else {
                    float4 ratio0 = lookupRatio(ratioIndex);
                    float4 ratio1 = lookupRatio(ratioIndex + 1);
                    ratio = lerpf<float4>(ratio0, ratio1, ratioFine);
                }
                float4 modfm =
                    params[MOD_FM_PARAM].getValue() * inputs[MOD_FM_INPUT].getPolyVoltageSimd<float4>(c) * dsp::FREQ_C4;
                freqModulator = (freqCarrier * ratio) + modfm;
            } else {
                // fixed
                float4 pitch = modOctave + modFine + inputs[MOD_PITCH_INPUT].getPolyVoltageSimd<float4>(c);
                float4 fm = params[MOD_FM_PARAM].getValue() * inputs[MOD_FM_INPUT].getPolyVoltageSimd<float4>(c);
                freqModulator = dsp::FREQ_C4 * (dsp::exp2_taylor5(pitch) + fm);
            }

            float4 level = simd::clamp(
                params[LEVEL_PARAM].getValue() +
                    (inputs[LEVEL_INPUT].getPolyVoltageSimd<float4>(c) * 0.1f * params[LEVEL_CV_PARAM].getValue()),
                0.0f, 1.0f);
            float4 falloff = level * 0.9f;  // numerical instability/volume issues closer to 1.0f

            float4 out1, out2;
            for (int32_t i = 0; i < channelsInBlock; ++i) {
                auto& osc = oscs[c + i];

                // TODO: vectorize dsp library
                osc.SetFreqCarrier(freqCarrier[i]);
                osc.SetFreqModulator(freqModulator[i]);
                osc.SetFalloff(falloff[i]);
                osc.Process();
                out1[i] = osc.Formula1();
                out2[i] = osc.Formula3();
            }

            // scale/clip
            out1 = 5.0f * approxTanhf(out1 * 0.8f);
            out2 = 5.0f * approxTanhf(out2 * 0.8f);

            outputs[OUT_MAIN_OUTPUT].setVoltageSimd<float4>(out1, c);
            outputs[OUT_AUX_OUTPUT].setVoltageSimd<float4>(out2, c);
        }

        // Set the number of channels for each output.
        outputs[OUT_MAIN_OUTPUT].setChannels(channels);
        outputs[OUT_AUX_OUTPUT].setChannels(channels);
    }

    DsfOscillator oscs[16];
    int32_t mLastChannels{};
};

struct DsfWidget : ModuleWidget, SvgHelper<DsfWidget> {
    DsfWidget(Dsf* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/DSF_Oscillator.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        bindParam<NKK2>("switch-modmode", Dsf::MOD_MODE_PARAM);
        bindParam<Davies1900hChocoKnob>("knob-modcoarse",Dsf::MOD_COARSE_PARAM);
        bindParam<Davies1900hForestKnob>("knob-coarse",Dsf::COARSE_PARAM);
        bindParam<Davies1900hChocoKnob>("knob-modfine",Dsf::MOD_FINE_PARAM);
        bindParam<Davies1900hChocoKnob>("knob-modlevel",Dsf::LEVEL_PARAM);
        bindParam<Davies1900hForestKnob>("knob-fine",Dsf::FINE_PARAM);
        bindParam<Trimpot>("trimpot-fm",Dsf::FM_PARAM);
        bindParam<Trimpot>("trimpot-modfm",Dsf::MOD_FM_PARAM);
        bindParam<Trimpot>("trimpot-modlevel",Dsf::LEVEL_CV_PARAM);

        bindInput<PJ301MPort>("in-fm",Dsf::FM_INPUT);
        bindInput<PJ301MPort>("in-modfm",Dsf::MOD_FM_INPUT);
        bindInput<PJ301MPort>("in-modlevel",Dsf::LEVEL_INPUT);
        bindInput<PJ301MPort>("in-pitch",Dsf::PITCH_INPUT);
        bindInput<PJ301MPort>("in-modpitch",Dsf::MOD_PITCH_INPUT);

        bindOutput<PJ301MPort>("out-main",Dsf::OUT_MAIN_OUTPUT);
        bindOutput<PJ301MPort>("out-aux",Dsf::OUT_AUX_OUTPUT);
    }
};

Model* modelDsf = createModel<Dsf, DsfWidget>("dsf");