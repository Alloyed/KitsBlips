#include "plugin.hpp"
#include "dsp/dsfOscillator.h"
#include "dsp/util.h"

struct Dsf : Module
{
	enum ParamId
	{
		MOD_TYPE_PARAM,
		MOD_PARAM,
		FREQ_PARAM,
		MOD_FM_PARAM,
		FM_PARAM,
		FALLOFF_PARAM,
		PARAMS_LEN
	};
	enum InputId
	{
		MOD_CV_INPUT,
		FREQ_CV_INPUT,
		MOD_FM_CV_INPUT,
		FM_CV_INPUT,
		FALLOFF_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId
	{
		OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId
	{
		LIGHTS_LEN
	};

	Dsf()
	{
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(FREQ_PARAM, -5.f, 5.f, 0.f, "Carrier frequency", " Hz", 2, dsp::FREQ_C4);
		configInput(FREQ_CV_INPUT, "V/Oct");
		configParam(FM_PARAM, -1.f, 1.f, 0.f, "FM");
		configInput(FM_CV_INPUT, "FM");
		configSwitch(MOD_TYPE_PARAM, 0.f, 1.f, 0.f, "Mod type", {"Ratio", "Fixed"});
		configParam(MOD_PARAM, 0.1f, 8.f, 1.f, "Modulation Frequency");
		configInput(MOD_CV_INPUT, "Modulation Frequency");
		configParam(MOD_FM_PARAM, -1.f, 1.f, 0.f, "Mod FM");
		configInput(MOD_FM_CV_INPUT, "Mod FM");
		configParam(FALLOFF_PARAM, 0.0f, .9f, 0.f, "Falloff");
		configInput(FALLOFF_CV_INPUT, "Falloff");
		configOutput(OUT_OUTPUT, "");
	}

	void onSampleRateChange(const SampleRateChangeEvent &e) override
	{
		Module::onSampleRateChange(e);
		for (auto &osc : oscs)
		{
			osc.Init(e.sampleRate);
		}
	}

	void process(const ProcessArgs &args) override
	{
		// Get desired number of channels from a "primary" input.
		// If this input is unpatched, getChannels() returns 0, but we should still generate 1 channel of output.
		int channels = std::max(1, inputs[FREQ_CV_INPUT].getChannels());

		// Iterate through each active channel.
		for (int c = 0; c < channels; c++)
		{
			float pitch = params[FREQ_PARAM].getValue() + inputs[FREQ_CV_INPUT].getPolyVoltage(c);
			float fm = params[FM_PARAM].getValue() * inputs[FM_CV_INPUT].getPolyVoltage(c);
			float freqCarrier = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch + fm);

			float freqModulator = 0.0f;
			if (params[MOD_TYPE_PARAM].getValue() == 0.0f)
			{
				// ratio
				float modpitch = params[MOD_PARAM].getValue() + inputs[MOD_CV_INPUT].getPolyVoltage(c);
				float modfm = params[MOD_FM_PARAM].getValue() * inputs[MOD_FM_CV_INPUT].getPolyVoltage(c);
				freqModulator = freqCarrier * (modpitch + modfm);
			}
			else
			{
				// fixed
				float modpitch = params[MOD_PARAM].getValue() + inputs[MOD_CV_INPUT].getPolyVoltage(c);
				float modfm = params[MOD_FM_PARAM].getValue() * inputs[MOD_FM_CV_INPUT].getPolyVoltage(c);
				freqModulator = dsp::FREQ_C4 * dsp::exp2_taylor5(modpitch + modfm);
			}

			float falloff = clampf(params[FALLOFF_PARAM].getValue() + (inputs[FALLOFF_CV_INPUT].getPolyVoltage(c) * 0.1f), 0.0f, 0.9f);

			auto &osc = oscs[c];

			// Set the c'th channel by passing the second argument.
			osc.SetFreqCarrier(freqCarrier);
			osc.SetFreqModulator(freqModulator);
			osc.SetFalloff(falloff);

			outputs[OUT_OUTPUT].setVoltage(5.0f * osc.Process(), c);
		}

		// Set the number of channels for each output.
		outputs[OUT_OUTPUT].setChannels(channels);
	}

	DsfOscillator oscs[16];
};

struct DsfWidget : ModuleWidget
{
	DsfWidget(Dsf *module)
	{
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/dsf.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<NKK>(mm2px(Vec(40.956, 31.362)), module, Dsf::MOD_TYPE_PARAM));
		addParam(createParamCentered<Davies1900hBlackKnob>(mm2px(Vec(40.701, 40.025)), module, Dsf::MOD_PARAM));
		addParam(createParamCentered<Davies1900hBlackKnob>(mm2px(Vec(11.879, 44.867)), module, Dsf::FREQ_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(42.1, 66.083)), module, Dsf::MOD_FM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(11.734, 75.843)), module, Dsf::FM_PARAM));
		addParam(createParamCentered<Davies1900hBlackKnob>(mm2px(Vec(31.626, 96.437)), module, Dsf::FALLOFF_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(41.072, 50.368)), module, Dsf::MOD_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(12.181, 59.95)), module, Dsf::FREQ_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(42.11, 76.471)), module, Dsf::MOD_FM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(11.744, 86.23)), module, Dsf::FM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(42.894, 96.632)), module, Dsf::FALLOFF_CV_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(59.939, 115.969)), module, Dsf::OUT_OUTPUT));
	}
};

Model *modelDsf = createModel<Dsf, DsfWidget>("dsf");