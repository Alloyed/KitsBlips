#include "kitdsp/resampler.h"
#include "kitdsp/psxReverb.h"
#include "plugin.hpp"

namespace
{
	constexpr size_t psxBufferSize = 65536; // PSX::GetBufferDesiredSizeFloats(PSX::kOriginalSampleRate);
	float psxBuffer[psxBufferSize];
	PSX::Model psx(PSX::kOriginalSampleRate, psxBuffer, psxBufferSize);
	Resampler psxSampler(PSX::kOriginalSampleRate, PSX::kOriginalSampleRate);
}

struct PSXVerb : Module
{
	enum ParamId
	{
		MIX_PARAM,
		MIX_AMT_PARAM,
		PARAMS_LEN
	};
	enum InputId
	{
		MIX_IN_INPUT,
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
		LIGHTS_LEN
	};

	PSXVerb()
	{
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(MIX_PARAM, 0.f, 1.f, 0.f, "Mix");
		configParam(MIX_AMT_PARAM, 0.f, 1.f, 0.f, "Mix CV");
		configInput(MIX_IN_INPUT, "Mix");
		configInput(AUDIO_L_INPUT, "Audio In (L/Mono)");
		configInput(AUDIO_R_INPUT, "Audio In (R) (normalled to L)");
		configOutput(AUDIO_L_OUTPUT, "OUT/L");
		configOutput(AUDIO_R_OUTPUT, "OUT/R");
	}

	void onSampleRateChange(const SampleRateChangeEvent &e) override
	{
		Module::onSampleRateChange(e);
		psxSampler = {PSX::kOriginalSampleRate, e.sampleRate};
	}

	void process(const ProcessArgs &args) override
	{
		float wetDry = params[MIX_PARAM].getValue();
		wetDry += params[MIX_AMT_PARAM].getValue() * inputs[MIX_IN_INPUT].getVoltage() * 0.1f;
		wetDry = wetDry > 1.0f ? 1.0f : wetDry < 0.0f ? 0.0f
													  : wetDry;

		float inputLeft = inputs[AUDIO_L_INPUT].getVoltage() / 5.0f;
		float inputRight = inputs[AUDIO_R_INPUT].isConnected() ? inputs[AUDIO_R_INPUT].getVoltage() / 5.0f : inputLeft;
		float psxLeft, psxRight;

		psxSampler.Process(inputLeft, inputRight, psxLeft, psxRight,
						   Resampler::InterpolationStrategy::Cubic,
						   [](float inLeft, float inRight, float &outLeft, float &outRight)
						   {
							   psx.Process(inLeft, inRight, outLeft, outRight);
						   });

		outputs[AUDIO_L_OUTPUT].setVoltage(5.0f * lerpf(inputLeft, psxLeft, wetDry));
		outputs[AUDIO_R_OUTPUT].setVoltage(5.0f * lerpf(inputRight, psxRight, wetDry));
	}
};

struct PSXVerbWidget : ModuleWidget
{
	PSXVerbWidget(PSXVerb *module)
	{
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/psxverb.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(39.539, 55.794)), module, PSXVerb::MIX_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(31.032, 85.505)), module, PSXVerb::MIX_AMT_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(31.388, 98.3)), module, PSXVerb::MIX_IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.419, 112.184)), module, PSXVerb::AUDIO_L_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.585, 112.184)), module, PSXVerb::AUDIO_R_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(31.388, 111.95)), module, PSXVerb::AUDIO_L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(43.555, 111.95)), module, PSXVerb::AUDIO_R_OUTPUT));
	}
};

Model *modelPSXVerb = createModel<PSXVerb, PSXVerbWidget>("psxverb");