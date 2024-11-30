#include "kitdsp/psxReverb.h"
#include "kitdsp/psxReverbPresets.h"
#include "kitdsp/resampler.h"
#include "plugin.hpp"

using namespace kitdsp;

namespace {
constexpr size_t psxBufferSize =
    65536;  // PSX::GetBufferDesiredSizeFloats(PSX::kOriginalSampleRate);
float psxBuffer[psxBufferSize];
PSX::Reverb psx(PSX::kOriginalSampleRate, psxBuffer, psxBufferSize);
kitdsp::Resampler<float_2> psxSampler(PSX::kOriginalSampleRate,
                                      PSX::kOriginalSampleRate);
size_t filterPreset = 1;
rack::dsp::SchmittTrigger nextFilter;
}  // namespace

struct PSXVerb : Module {
    enum ParamId { MIX_PARAM, ALGORITHM_PARAM, PARAMS_LEN };
    enum InputId { MIX_CV_INPUT, AUDIO_L_INPUT, AUDIO_R_INPUT, INPUTS_LEN };
    enum OutputId { AUDIO_L_OUTPUT, AUDIO_R_OUTPUT, OUTPUTS_LEN };
    enum LightId { PRESET_1_LIGHT, PRESET_2_LIGHT, PRESET_3_LIGHT, PRESET_4_LIGHT, LIGHTS_LEN };

    PSXVerb() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(MIX_PARAM, 0.f, 1.f, 0.f, "Mix", "%", 0.0f, 100.0f);
        configButton(ALGORITHM_PARAM, "Algorithm");
        configInput(MIX_CV_INPUT, "Mix");
        configInput(AUDIO_L_INPUT, "Audio In/L");
        configInput(AUDIO_R_INPUT, "Audio In/R");
        configOutput(AUDIO_L_OUTPUT, "OUT/L");
        configOutput(AUDIO_R_OUTPUT, "OUT/R");
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override {
        Module::onSampleRateChange(e);
        psxSampler = {PSX::kOriginalSampleRate, e.sampleRate};
    }

    void process(const ProcessArgs& args) override {
        if (nextFilter.process(params[ALGORITHM_PARAM].getValue())) {
            // intentionally 1-indexed
            filterPreset = (filterPreset % PSX::kNumPresets - 1) + 1;
            psx.cfg.preset = filterPreset;
        }
        lights[PRESET_1_LIGHT].setBrightness(filterPreset & 1);
        lights[PRESET_2_LIGHT].setBrightness(filterPreset & 2 >> 1);
        lights[PRESET_3_LIGHT].setBrightness(filterPreset & 4 >> 2);
        lights[PRESET_4_LIGHT].setBrightness(filterPreset & 8 >> 3);

        float wetDryMix = params[MIX_PARAM].getValue() + inputs[MIX_CV_INPUT].getVoltage() * 0.1f;
        wetDryMix = clamp(wetDryMix, 0.0f, 1.0f);

        float inputLeft = inputs[AUDIO_L_INPUT].getVoltage() / 5.0f;
        float_2 in = {inputLeft,
                      inputs[AUDIO_R_INPUT].isConnected()
                          ? inputs[AUDIO_R_INPUT].getVoltage() / 5.0f
                          : inputLeft};
        float_2 out;

        out = psxSampler.Process<
            kitdsp::InterpolationStrategy::Cubic>(
            in, [](float_2 in, float_2& out) { out = psx.Process(in); });

        outputs[AUDIO_L_OUTPUT].setVoltage(5.0f *
                                           lerpf(in.left, out.left, wetDryMix));
        outputs[AUDIO_R_OUTPUT].setVoltage(5.0f *
                                           lerpf(in.right, out.right, wetDryMix));
    }
};

struct PSXVerbWidget : ModuleWidget {
    PSXVerbWidget(PSXVerb* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/psxverb.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(
            Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(
            Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(
            createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH,
                                          RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        float x0 = 8;
        float x1 = 35;
        float x2 = 45;
        float ynext = 10;
        float y = 10;

        addLabel(mm2px(Vec(x0, y)), "Mix");
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x1, y)), module, PSXVerb::MIX_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x2, y)), module, PSXVerb::MIX_CV_INPUT));
        y += ynext;
        addLabel(mm2px(Vec(x0, y)), "Algo");
        addParam(createParamCentered<VCVButton>(mm2px(Vec(x1, y)), module, PSXVerb::ALGORITHM_PARAM));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(x2 - 2, y - 2)), module, PSXVerb::PRESET_1_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(x2 + 2, y - 2)), module, PSXVerb::PRESET_2_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(x2 - 2, y + 2)), module, PSXVerb::PRESET_3_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(x2 + 2, y + 2)), module, PSXVerb::PRESET_4_LIGHT));
        y += ynext;

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8, 115.969)), module, PSXVerb::AUDIO_L_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(13, 115.969)), module, PSXVerb::AUDIO_R_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(47.773, 115.969)), module, PSXVerb::AUDIO_L_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(53, 115.969)), module, PSXVerb::AUDIO_R_OUTPUT));
    }

    Label* addLabel(const Vec& v, const std::string& str) {
        NVGcolor black = nvgRGB(0, 0, 0);
        Label* label = new Label();
        label->box.pos = v;
        label->box.pos.y -= 5.f;
        label->text = str;
        label->color = black;
        addChild(label);
        return label;
    }
};

Model* modelPSXVerb = createModel<PSXVerb, PSXVerbWidget>("psxverb");