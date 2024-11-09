#include "plugin.hpp"
#include "dsp/dsfOscillator.h"
#include "dsp/util.h"

struct NKK2 : app::SvgSwitch
{
	NKK2()
	{
		shadow->opacity = 0.0;
		// addFrame(Svg::load(asset::system("res/ComponentLibrary/NKK_1.svg")));
		addFrame(Svg::load(asset::system("res/ComponentLibrary/NKK_2.svg")));
		addFrame(Svg::load(asset::system("res/ComponentLibrary/NKK_0.svg")));
	}
};

// we need one secret -1 and +1 ratio for the fine tune knob
static constexpr float cModulatorRatios[] = {0.0f,
											 0.25f,
											 0.5f,
											 1.f,
											 2.f,
											 3.f,
											 4.f,
											 5.f,
											 10.0f};
static constexpr float cNumRatios = sizeof(cModulatorRatios) - 2.0f;
static constexpr float cMaxLevel = 0.9f;

struct Dsf : Module
{
	enum ParamId
	{
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
	enum InputId
	{
		PITCH_INPUT,
		FM_INPUT,
		MOD_PITCH_INPUT,
		MOD_FM_INPUT,
		LEVEL_INPUT,
		INPUTS_LEN
	};
	enum OutputId
	{
		OUT_MAIN_OUTPUT,
		OUT_AUX_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId
	{
		LIGHTS_LEN
	};

	Dsf()
	{
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(COARSE_PARAM, -2.f, 4.f, 0.f, "Octave");
		configParam(FINE_PARAM, -7.f, 7.f, 0.f, "Fine (+/- 7 semitones)");
		configParam(FM_PARAM, -1.f, 1.f, 0.f, "FM (linear)");
		configParam(MOD_MODE_PARAM, 0.f, 1.f, 0.f, "Ratio/Fixed pitch");
		configParam(MOD_COARSE_PARAM, -2.f, 4.f, 0.f, "Coarse");
		configParam(MOD_FINE_PARAM, -7.f, 7.f, 0.f, "Fine");
		configParam(MOD_FM_PARAM, -1.f, 1.f, 0.f, "FM (linear)");
		configParam(LEVEL_PARAM, 0.f, 1.0f, 0.5f, "Harmonics Level");
		configParam(LEVEL_CV_PARAM, -1.f, 1.f, 0.f, "Harmonics CV scaling");
		configInput(PITCH_INPUT, "Pitch");
		configInput(MOD_PITCH_INPUT, "Harmonics");
		configInput(FM_INPUT, "FM");
		configInput(MOD_FM_INPUT, "Harmonics FM");
		configInput(LEVEL_INPUT, "Level");
		configOutput(OUT_MAIN_OUTPUT, "Main out");
		configOutput(OUT_AUX_OUTPUT, "Aux out");
	}

	void onReset(const ResetEvent &e) override
	{
		Module::onReset(e);
		for (auto &osc : oscs)
		{
			osc.Reset();
		}
	}

	void onSampleRateChange(const SampleRateChangeEvent &e) override
	{
		Module::onSampleRateChange(e);
		for (auto &osc : oscs)
		{
			osc.Init(e.sampleRate);
		}
	}

	void
	process(const ProcessArgs &args) override
	{
		// Get desired number of channels from a "primary" input.
		// If this input is unpatched, getChannels() returns 0, but we should still generate 1 channel of output.
		int channels = std::max(1, inputs[PITCH_INPUT].getChannels());

		// Iterate through each active channel.
		for (int c = 0; c < channels; c++)
		{
			float freqCarrier = 0.0f;
			{
				float octave = floorf(params[COARSE_PARAM].getValue() + 0.5f);
				float pitch = octave + inputs[PITCH_INPUT].getPolyVoltage(c) + (params[FINE_PARAM].getValue() / 12.0f);
				float fm = params[FM_PARAM].getValue() * inputs[FM_INPUT].getPolyVoltage(c);
				freqCarrier = dsp::FREQ_C4 * (dsp::exp2_taylor5(pitch) + fm);
			}

			float freqModulator = 0.0f;
			if (params[MOD_MODE_PARAM].getValue() == 0.0f)
			{
				// ratio
				float ratioCoarse = clampf(params[MOD_COARSE_PARAM].getValue() + 2.0f + inputs[MOD_PITCH_INPUT].getPolyVoltage(c), 0.0f, 6.0f);
				int32_t ratioIndex = static_cast<int32_t>(ratioCoarse + 0.5f) + 1;
				float ratioFine = params[MOD_FINE_PARAM].getValue() / 14.0f;
				float ratio;
				if (ratioFine < 0.0f)
				{
					// negative
					ratio = lerpf(cModulatorRatios[ratioIndex - 1], cModulatorRatios[ratioIndex], -ratioFine);
				}
				else
				{
					ratio = lerpf(cModulatorRatios[ratioIndex], cModulatorRatios[ratioIndex + 1], ratioFine);
				}
				float modfm = params[MOD_FM_PARAM].getValue() * inputs[MOD_FM_INPUT].getPolyVoltage(c) * dsp::FREQ_C4;
				freqModulator = (freqCarrier * ratio) + modfm;
			}
			else
			{
				// fixed
				float octave = floorf(params[MOD_COARSE_PARAM].getValue() + 0.5f);
				float fine = (params[MOD_FINE_PARAM].getValue() / 12.0f);
				float pitch = octave + fine + inputs[MOD_PITCH_INPUT].getPolyVoltage(c);
				float fm = params[MOD_FM_PARAM].getValue() * inputs[MOD_FM_INPUT].getPolyVoltage(c);
				freqModulator = dsp::FREQ_C4 * (dsp::exp2_taylor5(pitch) + fm);
			}

			float level = clampf(params[LEVEL_PARAM].getValue() + (inputs[LEVEL_INPUT].getPolyVoltage(c) * 0.1f * params[LEVEL_CV_PARAM].getValue()), 0.0f, 1.0f);
			float falloff = level * 0.9f; // numerical instability/volume issues closer to 1.0f

			auto &osc = oscs[c];

			osc.SetFreqCarrier(freqCarrier);
			osc.SetFreqModulator(freqModulator);
			osc.SetFalloff(falloff);
			float out1, out2;
			osc.Process(out1, out2);

			// scale/clip
			out1 = 5.0f * tanhf(out1 * 0.8f);
			out2 = 5.0f * tanhf(out2 * 0.8f);

			outputs[OUT_MAIN_OUTPUT].setVoltage(out1, c);
			outputs[OUT_AUX_OUTPUT].setVoltage(out2, c);
		}

		// Set the number of channels for each output.
		outputs[OUT_MAIN_OUTPUT].setChannels(channels);
		outputs[OUT_AUX_OUTPUT].setChannels(channels);
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

		addParam(createParamCentered<NKK2>(mm2px(Vec(55.239, 29.905)), module, Dsf::MOD_MODE_PARAM));
		addParam(createParamCentered<Davies1900hBlackKnob>(mm2px(Vec(35.414, 31.587)), module, Dsf::MOD_COARSE_PARAM));
		addParam(createParamCentered<Davies1900hBlackKnob>(mm2px(Vec(13.026, 31.956)), module, Dsf::COARSE_PARAM));
		addParam(createParamCentered<Davies1900hBlackKnob>(mm2px(Vec(35.613, 51.719)), module, Dsf::MOD_FINE_PARAM));
		addParam(createParamCentered<Davies1900hBlackKnob>(mm2px(Vec(55.873, 51.719)), module, Dsf::LEVEL_PARAM));
		addParam(createParamCentered<Davies1900hBlackKnob>(mm2px(Vec(11.775, 52.419)), module, Dsf::FINE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(11.777, 72.881)), module, Dsf::FM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(35.667, 72.881)), module, Dsf::MOD_FM_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(56.075, 72.881)), module, Dsf::LEVEL_CV_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(11.773, 90.193)), module, Dsf::FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(35.663, 90.193)), module, Dsf::MOD_FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(55.92, 90.193)), module, Dsf::LEVEL_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.013, 112.103)), module, Dsf::PITCH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30.258, 112.103)), module, Dsf::MOD_PITCH_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(45.424, 112.103)), module, Dsf::OUT_MAIN_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(57.366, 112.103)), module, Dsf::OUT_AUX_OUTPUT));
	}
};

Model *modelDsf = createModel<Dsf, DsfWidget>("dsf");