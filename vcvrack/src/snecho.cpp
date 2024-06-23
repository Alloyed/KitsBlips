#include "dsp/resampler.h"
#include "dsp/snesEcho.h"
#include "plugin.hpp"

#define SAMPLE_RATE 32000

namespace
{
    constexpr size_t snesBufferSize = 7680UL;
    int16_t snesBuffer[7680];
    SNES::Model snes(SNES::kOriginalSampleRate, snesBuffer, snesBufferSize);
    Resampler snesSampler(SNES::kOriginalSampleRate, SAMPLE_RATE);
} // namespace

struct Snecho : Module
{
    enum ParamId
    {
        SIZE_PARAM,
        FEEDBACK_PARAM,
        FILTER_PARAM,
        MIX_PARAM,
        SIZE_AMT_PARAM,
        FILTER_AMT_PARAM,
        FEEDBACK_AMT_PARAM,
        MIX_AMT_PARAM,
        PARAMS_LEN
    };
    enum InputId
    {
        SIZE_IN_INPUT,
        FILTER_IN_INPUT,
        MIX_IN_INPUT,
        FEEDBACK_IN_INPUT,
        AUDIO_IN_INPUT,
        INPUTS_LEN
    };
    enum OutputId
    {
        AUDIO_L_OUTPUT,
        AUDIO_R_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId
    {
        LIGHTS_LEN
    };

    Snecho()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(SIZE_PARAM, 0.f, 1.f, 0.5f, "Size");
        configParam(FEEDBACK_PARAM, 0.f, 1.f, 0.f, "Feedback");
        configParam(FILTER_PARAM, 0.f, 1.f, 0.f, "Filter");
        configParam(MIX_PARAM, 0.f, 1.f, 0.5f, "Mix");
        configParam(SIZE_AMT_PARAM, -1.f, 1.f, 0.f, "Size In Amount");
        configParam(FILTER_AMT_PARAM, -1.f, 1.f, 0.f, "Filter In Amount");
        configParam(FEEDBACK_AMT_PARAM, -1.f, 1.f, 0.f, "Feedback In Amount");
        configParam(MIX_AMT_PARAM, -1.f, 1.f, 0.f, "Mix In Amount");
        configInput(SIZE_IN_INPUT, "Size In");
        configInput(FILTER_IN_INPUT, "Filter In");
        configInput(MIX_IN_INPUT, "Mix In");
        configInput(FEEDBACK_IN_INPUT, "Feedback In");
        configInput(AUDIO_IN_INPUT, "Audio In");
        configOutput(AUDIO_L_OUTPUT, "Out/L");
        configOutput(AUDIO_R_OUTPUT, "Out/R");

        snesSampler = {SNES::kOriginalSampleRate, 48000};
    }

    void onSampleRateChange(const SampleRateChangeEvent &e) override
    {
        Module::onSampleRateChange(e);
        snesSampler = {SNES::kOriginalSampleRate, static_cast<int32_t>(e.sampleRate)};
    }

    void process(const ProcessArgs &args) override
    {
        snes.cfg.echoBufferSize = params[SIZE_PARAM].getValue();
        snes.mod.echoBufferSize = params[SIZE_AMT_PARAM].getValue() * inputs[SIZE_IN_INPUT].getVoltage() * 0.1f;
        snes.cfg.echoDelayMod = 1.0f; // TODO
        snes.mod.echoDelayMod = 0.0f; // TODO
        snes.mod.freezeEcho = 0.0f;   // TODO
        snes.cfg.echoFeedback = params[FEEDBACK_PARAM].getValue();
        snes.mod.echoFeedback = params[FEEDBACK_AMT_PARAM].getValue() * inputs[FEEDBACK_IN_INPUT].getVoltage() * 0.1f;
        snes.cfg.filterSetting = 0; // TODO
        snes.cfg.filterMix = params[FILTER_PARAM].getValue();
        snes.mod.filterMix = params[FILTER_AMT_PARAM].getValue() * inputs[FILTER_IN_INPUT].getVoltage() * 0.1f;

        float wetDry = params[MIX_PARAM].getValue();
        wetDry += params[MIX_AMT_PARAM].getValue() * inputs[MIX_IN_INPUT].getVoltage() * 0.1f;
        wetDry = wetDry > 1.0f ? 1.0f : wetDry < 0.0f ? 0.0f
                                                      : wetDry;

        float input = inputs[AUDIO_IN_INPUT].getVoltage() / 5.0f;
        float snesLeft, snesRight;

        snesSampler.Process(input, input, snesLeft, snesRight,
                            [](float inLeft, float inRight, float &outLeft, float &outRight)
                            {
                                snes.Process(inLeft, inRight, outLeft, outRight);
                            });

        outputs[AUDIO_L_OUTPUT].setVoltage(5.0f * lerpf(input, snesLeft, wetDry));
        outputs[AUDIO_R_OUTPUT].setVoltage(5.0f * lerpf(input, snesRight, wetDry));
    }
};

struct SnechoWidget : ModuleWidget
{
    SnechoWidget(Snecho *module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/snecho.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<Rogan1PWhite>(mm2px(Vec(11.165, 36.477)), module, Snecho::SIZE_PARAM));
        addParam(createParamCentered<Rogan1PWhite>(mm2px(Vec(39.539, 36.477)), module, Snecho::FEEDBACK_PARAM));
        addParam(createParamCentered<Rogan1PWhite>(mm2px(Vec(11.051, 55.626)), module, Snecho::FILTER_PARAM));
        addParam(createParamCentered<Rogan1PWhite>(mm2px(Vec(39.539, 55.794)), module, Snecho::MIX_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(6.698, 85.282)), module, Snecho::SIZE_AMT_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(18.976, 85.505)), module, Snecho::FILTER_AMT_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(43.534, 85.282)), module, Snecho::FEEDBACK_AMT_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(31.032, 85.505)), module, Snecho::MIX_AMT_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.043, 98.3)), module, Snecho::SIZE_IN_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(19.222, 98.3)), module, Snecho::FILTER_IN_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(31.388, 98.3)), module, Snecho::MIX_IN_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(43.591, 98.3)), module, Snecho::FEEDBACK_IN_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.044, 111.95)), module, Snecho::AUDIO_IN_INPUT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(31.388, 111.95)), module, Snecho::AUDIO_L_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(43.555, 111.95)), module, Snecho::AUDIO_R_OUTPUT));
    }
};

Model *modelSnecho = createModel<Snecho, SnechoWidget>("snecho");