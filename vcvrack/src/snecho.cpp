#include "kitdsp/resampler.h"
#include "kitdsp/snesEcho.h"
#include "kitdsp/snesEchoFilterPresets.h"
#include "plugin.hpp"

#define SAMPLE_RATE 32000

using namespace kitdsp;

namespace {
constexpr size_t snesBufferSize = 7680UL;
int16_t snesBuffer1[7680];
SNES::Echo snes1(snesBuffer1, snesBufferSize);
Resampler<float> snesSampler(SNES::kOriginalSampleRate, SAMPLE_RATE);
size_t filterPreset = 0;
rack::dsp::SchmittTrigger nextFilter;
rack::dsp::SchmittTrigger freeze;
rack::dsp::SchmittTrigger clear;
rack::dsp::SchmittTrigger reset;
rack::dsp::SchmittTrigger clockIn;
rack::dsp::Timer clockTime;
}  // namespace

struct DelayQuantity : public rack::engine::ParamQuantity {
    void setDisplayValueString(std::string s) override {
        auto f = std::atof(s.c_str());
        if (f > 0) {
            setValue(f);
        } else {
            setValue(0);
        }
    }

    virtual std::string getDisplayValueString() override { return rack::string::f("%f", getValue()); }
};

struct Snecho : Module {
    enum ParamId {
        SIZE_PARAM,
        FEEDBACK_PARAM,
        FILTER_PRESET_PARAM,
        MIX_PARAM,
        FREEZE_PARAM,
        MOD_PARAM,
        FILTER_MIX_PARAM,
        SIZE_RANGE_PARAM,
        CLEAR_PARAM,
        RESET_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        SIZE_CV_INPUT,
        FEEDBACK_CV_INPUT,
        MIX_CV_INPUT,
        FREEZE_GATE_INPUT,
        MOD_CV_INPUT,
        FILTER_MIX_CV_INPUT,
        CLEAR_TRIG_INPUT,
        RESET_TRIG_INPUT,
        AUDIO_INPUT,
        CLOCK_INPUT,
        INPUTS_LEN
    };
    enum OutputId { AUDIO_L_OUTPUT, AUDIO_R_OUTPUT, OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    Snecho() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(SIZE_PARAM, 0.f, 1.f, 0.f, "Echo buffer size");
        configParam(FEEDBACK_PARAM, -1.f, 1.f, 0.f, "Feedback", "%", 0.0f, 100.0f);
        configButton(FILTER_PRESET_PARAM, "Swap out FIR Filter setting");
        configParam(MIX_PARAM, 0.f, 1.f, 0.f, "Mix", "%", 0.0f, 100.0f);
        configSwitch(FREEZE_PARAM, 0.f, 1.f, 0.f, "Freeze Input");
        configParam(MOD_PARAM, 0.f, 1.f, 0.f, "Delay modulation/wiggle");
        configParam(FILTER_MIX_PARAM, 0.f, 1.f, 0.f, "Filter Mix", "%", 0.0f, 100.0f);
        configSwitch(SIZE_RANGE_PARAM, 0.f, 2.f, 0.f, "Buffer Size Range", {"Standard", "Medium", "Long"});
        configButton(CLEAR_PARAM, "Clear Delay Buffer");
        configButton(RESET_PARAM, "Reset playhead to start");

        configInput(SIZE_CV_INPUT, "Buffer size");
        configInput(FEEDBACK_CV_INPUT, "Feedback");
        configInput(MIX_CV_INPUT, "Mix");
        configInput(FREEZE_GATE_INPUT, "Freeze Input (gate)");
        configInput(MOD_CV_INPUT, "Delay mod/Wiggle");
        configInput(FILTER_MIX_CV_INPUT, "Filter Mix");
        configInput(CLEAR_TRIG_INPUT, "Clear Delay Buffer (trig)");
        configInput(RESET_TRIG_INPUT, "Reset playhead to start (trig)");
        configInput(AUDIO_INPUT, "Audio In");
        configInput(CLOCK_INPUT, "Clock In (used for delay sync)");

        configOutput(AUDIO_L_OUTPUT, "Audio L");
        configOutput(AUDIO_R_OUTPUT, "Audio R(Inverted)");

        configBypass(AUDIO_INPUT, AUDIO_L_OUTPUT);
        configBypass(AUDIO_INPUT, AUDIO_R_OUTPUT);
        snesSampler = {SNES::kOriginalSampleRate, 48000};
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override {
        Module::onSampleRateChange(e);
        snesSampler = {SNES::kOriginalSampleRate, e.sampleRate};
    }

    void process(const ProcessArgs& args) override {
        // inputs
        // core
        snes1.cfg.echoBufferSize = params[SIZE_PARAM].getValue();
        snes1.mod.echoBufferSize = inputs[SIZE_CV_INPUT].getVoltage() * 0.1f;

        snes1.cfg.echoFeedback = params[FEEDBACK_PARAM].getValue();
        snes1.mod.echoFeedback = inputs[FEEDBACK_CV_INPUT].getVoltage() * 0.1f;

        if (nextFilter.process(params[FILTER_PRESET_PARAM].getValue())) {
            filterPreset = filterPreset + 1 % sizeof(SNES::kFilterPresets);
            memcpy(snes1.cfg.filterCoefficients, SNES::kFilterPresets[filterPreset].data, 8);
        }

        float wetDryMix = clamp(params[MIX_PARAM].getValue() + inputs[MIX_CV_INPUT].getVoltage() * 0.1f, 0.0f, 1.0f);

        snes1.cfg.freezeEcho = (params[FREEZE_PARAM].getValue()) > 0.5f;
        freeze.process(inputs[FREEZE_GATE_INPUT].getVoltage(), .1f, 1.f);
        snes1.mod.freezeEcho = freeze.isHigh();

        // extension
        snes1.cfg.echoDelayMod = params[MOD_PARAM].getValue();
        snes1.mod.echoDelayMod = inputs[MOD_CV_INPUT].getVoltage() * 0.1f;

        snes1.cfg.filterMix = params[FILTER_MIX_PARAM].getValue();
        snes1.mod.filterMix = inputs[FILTER_MIX_CV_INPUT].getVoltage() * 0.1f;

        snes1.mod.clearBuffer =
            clear.process(params[CLEAR_PARAM].getValue() + inputs[CLEAR_TRIG_INPUT].getVoltage(), .1f, 1.f);
        snes1.mod.resetHead =
            clear.process(params[RESET_PARAM].getValue() + inputs[RESET_TRIG_INPUT].getVoltage(), .1f, 1.f);

        if (inputs[CLOCK_INPUT].isConnected()) {
            clockTime.process(args.sampleTime);
            if (clockIn.process(inputs[CLOCK_INPUT].getVoltage())) {
                snes1.cfg.echoBufferIncrementSamples = SNES::MsToSamples(clockTime.getTime() * 1000.0f);
                clockTime.reset();
            }
        } else {
            snes1.cfg.echoBufferIncrementSamples = SNES::kOriginalEchoIncrementSamples;
        }

        // processing
        float drySignal = inputs[AUDIO_INPUT].getVoltage() / 5.0f;
        float wetSignal = snesSampler.Process<kitdsp::InterpolationStrategy::None>(
            drySignal, [](float in, float& out) { out = snes1.Process(in * 0.5f) * 2.0f; });

        // outputs
        outputs[AUDIO_L_OUTPUT].setVoltage(5.0f * lerpf(drySignal, wetSignal, wetDryMix));
        outputs[AUDIO_R_OUTPUT].setVoltage(-5.0f * lerpf(drySignal, wetSignal, wetDryMix));
    }
};

struct SnechoWidget : ModuleWidget {
    SnechoWidget(Snecho* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/snecho.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        float x0 = 8;
        float x1 = 35;
        float x2 = 45;
        float ynext = 10;
        float y = 10;

        addLabel(mm2px(Vec(x0, y)), "size");
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x1, y)), module, Snecho::SIZE_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x2, y)), module, Snecho::SIZE_CV_INPUT));
        y += ynext;
        addLabel(mm2px(Vec(x0, y)), "fb");
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x1, y)), module, Snecho::FEEDBACK_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x2, y)), module, Snecho::FEEDBACK_CV_INPUT));
        y += ynext;
        addLabel(mm2px(Vec(x0, y)), "fir");
        addParam(createParamCentered<VCVButton>(mm2px(Vec(x1, y)), module, Snecho::FILTER_PRESET_PARAM));
        y += ynext;
        addLabel(mm2px(Vec(x0, y)), "mix");
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x1, y)), module, Snecho::MIX_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x2, y)), module, Snecho::MIX_CV_INPUT));
        y += ynext;
        addLabel(mm2px(Vec(x0, y)), "freeze");
        addParam(createParamCentered<CKSS>(mm2px(Vec(x1, y)), module, Snecho::FREEZE_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x2, y)), module, Snecho::FREEZE_GATE_INPUT));
        y += ynext;

        addLabel(mm2px(Vec(x0, y)), "(EX) mod");
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x1, y)), module, Snecho::MOD_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x2, y)), module, Snecho::MOD_CV_INPUT));
        y += ynext;
        addLabel(mm2px(Vec(x0, y)), "(EX) fir mix");
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(x1, y)), module, Snecho::FILTER_MIX_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x2, y)), module, Snecho::FILTER_MIX_CV_INPUT));
        y += ynext;
        addLabel(mm2px(Vec(x0, y)), "(EX) range");
        addParam(createParamCentered<CKSSThreeHorizontal>(mm2px(Vec(x1, y)), module, Snecho::SIZE_RANGE_PARAM));
        y += ynext;
        addLabel(mm2px(Vec(x0, y)), "(EX) clear");
        addParam(createParamCentered<VCVButton>(mm2px(Vec(x1, y)), module, Snecho::CLEAR_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x2, y)), module, Snecho::CLEAR_TRIG_INPUT));
        y += ynext;
        addLabel(mm2px(Vec(x0, y)), "(EX) reset");
        addParam(createParamCentered<VCVButton>(mm2px(Vec(x1, y)), module, Snecho::RESET_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x2, y)), module, Snecho::RESET_TRIG_INPUT));
        y += ynext;
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8, 115.969)), module, Snecho::AUDIO_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20, 115.969)), module, Snecho::CLOCK_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(47.773, 115.969)), module, Snecho::AUDIO_L_OUTPUT));
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

Model* modelSnecho = createModel<Snecho, SnechoWidget>("snecho");