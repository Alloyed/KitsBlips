#include "kitdsp/resampler.h"
#include "kitdsp/snesEcho.h"
#include "plugin.hpp"

#define SAMPLE_RATE 32000

namespace
{
    constexpr size_t snesBufferSize = 7680UL;
    int16_t snesBuffer1[7680];
    int16_t snesBuffer2[7680];
    SNES::Model snes1(SNES::kOriginalSampleRate, snesBuffer1, snesBufferSize);
    SNES::Model snes2(SNES::kOriginalSampleRate, snesBuffer2, snesBufferSize);
    kitdsp::Resampler snesSampler(SNES::kOriginalSampleRate, SAMPLE_RATE);
} // namespace

struct DelayQuantity : public rack::engine::ParamQuantity
{
    void setDisplayValueString(std::string s) override
    {
        auto f = std::atof(s.c_str());
        if (f > 0)
        {
            setValue(f);
        }
        else
        {
            setValue(0);
        }
    }

    virtual std::string getDisplayValueString() override
    {
        return rack::string::f("%f", getValue());
    }
};

struct Snecho : Module
{
    enum ParamId
    {
        FILTER_PARAM,
        FEEDBACK_PARAM,
        MIX_PARAM,
        DELAY_PARAM,
        DELAY_MOD_PARAM,
        FREEZE_PARAM,
        CLEAR_PARAM,
        RESET_PARAM,
        PARAMS_LEN
    };
    enum InputId
    {
        MIX_CV_INPUT,
        DELAY_MOD_CV_INPUT,
        FEEDBACK_CV_INPUT,
        DELAY_CV_INPUT,
        FREEZE_CV_INPUT,
        CLEAR_CV_INPUT,
        RESET_CV_INPUT,
        AUDIO_L_INPUT,
        AUDIO_R_INPUT,
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
        LIGHT0_LIGHT,
        LIGHT0G_LIGHT,
        LIGHT1_LIGHT,
        LIGHT1G_LIGHT,
        LIGHT2_LIGHT,
        LIGHT2G_LIGHT,
        LIGHT3_LIGHT,
        LIGHT3G_LIGHT,
        LIGHT4_LIGHT,
        LIGHT4G_LIGHT,
        LIGHTS_LEN
    };

    Snecho()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(FILTER_PARAM, 0.f, 1.f, 0.f, "");
        configParam(FEEDBACK_PARAM, -1.f, 1.f, 0.f, "Feedback", "%", 0.0f, 100.0f);
        configParam(MIX_PARAM, 0.f, 1.f, 0.f, "Mix", "%", 0.0f, 100.0f);
        configParam(DELAY_PARAM, 0.f, 1.f, 0.f, "Delay Length");
        configParam(DELAY_MOD_PARAM, 0.f, 1.f, 0.f, "Delay mod/wiggle");
        configParam(FREEZE_PARAM, 0.f, 1.f, 0.f, "Freeze Input");
        configParam(CLEAR_PARAM, 0.f, 1.f, 0.f, "Clear Delay Buffer");
        configParam(RESET_PARAM, 0.f, 1.f, 0.f, "Reset playhead to start");
        configInput(MIX_CV_INPUT, "Mix");
        configInput(DELAY_MOD_CV_INPUT, "Delay mod/Wiggle");
        configInput(FEEDBACK_CV_INPUT, "Feedback");
        configInput(DELAY_CV_INPUT, "Delay");
        configInput(FREEZE_CV_INPUT, "Freeze Input (gate)");
        configInput(CLEAR_CV_INPUT, "Clear Delay Buffer (trig)");
        configInput(RESET_CV_INPUT, "Reset playhead to start (trig)");
        configInput(AUDIO_L_INPUT, "Audio L");
        configInput(AUDIO_R_INPUT, "Audio R");
        configOutput(AUDIO_L_OUTPUT, "Audio L");
        configOutput(AUDIO_R_OUTPUT, "Audio R");
        configBypass(AUDIO_L_INPUT, AUDIO_L_OUTPUT);
        configBypass(AUDIO_R_INPUT, AUDIO_R_OUTPUT);
        snesSampler = {SNES::kOriginalSampleRate, 48000};
    }

    void onSampleRateChange(const SampleRateChangeEvent &e) override
    {
        Module::onSampleRateChange(e);
        snesSampler = {SNES::kOriginalSampleRate, e.sampleRate};
    }

    void process(const ProcessArgs &args) override
    {
        snes1.cfg.echoBufferSize = params[DELAY_PARAM].getValue();
        snes1.mod.echoBufferSize = inputs[DELAY_CV_INPUT].getVoltage() * 0.1f;
        snes1.cfg.echoDelayMod = params[DELAY_MOD_PARAM].getValue();
        snes1.mod.echoDelayMod = inputs[DELAY_MOD_CV_INPUT].getVoltage() * 0.1f;
        snes1.cfg.echoFeedback = params[FEEDBACK_PARAM].getValue();
        snes1.mod.echoFeedback = inputs[FEEDBACK_CV_INPUT].getVoltage() * 0.1f;
        snes1.cfg.filterSetting = 0; // TODO
        snes1.cfg.filterMix = 0.0f;
        snes1.mod.filterMix = 0.0f;

        // TODO: hysterisis
        snes1.cfg.freezeEcho = (params[FREEZE_PARAM].getValue()) > 0.5f;
        snes1.mod.freezeEcho = (inputs[FREEZE_CV_INPUT].getVoltage() * 0.1f) > 0.25f;
        snes1.mod.clearBuffer = (params[CLEAR_PARAM].getValue() > 0.5f) || (inputs[CLEAR_CV_INPUT].getVoltage() * 0.1f) > 0.25f;
        snes1.mod.resetHead = (params[RESET_PARAM].getValue() > 0.5f) || (inputs[RESET_CV_INPUT].getVoltage() * 0.1f) > 0.25f;

        snes2.cfg = snes1.cfg;
        snes2.mod = snes1.mod;

        float wetDry = params[MIX_PARAM].getValue();
        wetDry += inputs[MIX_CV_INPUT].getVoltage() * 0.1f;
        wetDry = wetDry > 1.0f ? 1.0f : wetDry < 0.0f ? 0.0f
                                                      : wetDry;

        float inputL = inputs[AUDIO_L_INPUT].getVoltage() / 5.0f;
        float inputR = inputs[AUDIO_R_INPUT].isConnected() ? inputs[AUDIO_R_INPUT].getVoltage() / 5.0f : inputL;
        float snesLeft, snesRight;

        snesSampler.Process<
            kitdsp::Resampler::InterpolationStrategy::Cubic>(inputL, inputR, snesLeft, snesRight,
                                                             [](float inLeft, float inRight, float &outLeft, float &outRight)
                                                             {
                                                                 float tmp;
                                                                 snes1.Process(inLeft, inLeft, outLeft, tmp);
                                                                 snes2.Process(inRight, inRight, outRight, tmp);
                                                             });

        outputs[AUDIO_L_OUTPUT].setVoltage(5.0f * lerpf(inputL, snesLeft, wetDry));
        outputs[AUDIO_R_OUTPUT].setVoltage(5.0f * lerpf(inputR, snesRight, wetDry));

        size_t i = 0;
        for (size_t idx = LIGHT0_LIGHT; idx < LIGHTS_LEN; idx += 2)
        {
            lights[idx].setBrightnessSmooth((static_cast<size_t>(snes1.viz.readHeadLocation * 5) == i ? 0.5 : 0), args.sampleTime);
            i++;
        }
        i = 0;
        for (size_t idx = LIGHT0G_LIGHT; idx < LIGHTS_LEN; idx += 2)
        {
            lights[idx].setBrightnessSmooth((static_cast<size_t>(snes1.viz.writeHeadLocation * 5) == ((i - 1) % 5) ? 0.5 : 0), args.sampleTime);
            i++;
        }
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

        addParam(createParamCentered<VCVButton>(mm2px(Vec(10.659, 23.553)), module, Snecho::FILTER_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(35.609, 39.825)), module, Snecho::FEEDBACK_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(21.831, 52.662)), module, Snecho::MIX_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50.808, 52.662)), module, Snecho::DELAY_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(35.609, 68.519)), module, Snecho::DELAY_MOD_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(20.033, 94.993)), module, Snecho::FREEZE_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(41.899, 94.993)), module, Snecho::CLEAR_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(60.984, 94.993)), module, Snecho::RESET_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(14.264, 83.942)), module, Snecho::MIX_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(26.431, 83.942)), module, Snecho::DELAY_MOD_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(40.209, 83.942)), module, Snecho::FEEDBACK_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(52.376, 83.942)), module, Snecho::DELAY_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(11.832, 102.249)), module, Snecho::FREEZE_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(32.706, 102.249)), module, Snecho::CLEAR_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(53.508, 102.249)), module, Snecho::RESET_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.866, 115.969)), module, Snecho::AUDIO_L_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.033, 115.969)), module, Snecho::AUDIO_R_INPUT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(47.773, 115.969)), module, Snecho::AUDIO_L_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(59.939, 115.969)), module, Snecho::AUDIO_R_OUTPUT));

        addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(43.117, 23.553)), module, Snecho::LIGHT0_LIGHT));
        addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(47.71, 23.553)), module, Snecho::LIGHT1_LIGHT));
        addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(52.302, 23.553)), module, Snecho::LIGHT2_LIGHT));
        addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(56.895, 23.553)), module, Snecho::LIGHT3_LIGHT));
        addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(61.488, 23.553)), module, Snecho::LIGHT4_LIGHT));
    }
};

Model *modelSnecho = createModel<Snecho, SnechoWidget>("snecho");