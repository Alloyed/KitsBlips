#include <clapeze/effectPlugin.h>
#include <clapeze/entryPoint.h>
#include <clapeze/features/assetsFeature.h>
#include <clapeze/features/params/enumParametersFeature.h>
#include <clapeze/features/params/parameterTypes.h>
#include <clapeze/features/state/tomlStateFeature.h>
#include <clapeze/processor/baseProcessor.h>
#include <kitdsp/math/util.h>

#include "descriptor.h"
#include "kitdsp/filters/biquad.h"
#include "kitdsp/filters/crossover.h"
#include "kitdsp/filters/svf.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include "gui/debugui.h"
#include "gui/kitguiFeature.h"
#endif

namespace {
enum class FilterType : clap_id {
    SvfLowpass,
    SvfHighpass,
    SvfBandpass,
    CrossoverLow,
    CrossoverHigh,
    BiquadLowPass,
};
enum class Params : clap_id { Type, Frequency, Q, Mix, Count };
using ParamsFeature = clapeze::params::EnumParametersFeature<Params>;
}  // namespace

namespace clapeze::params {

template <>
struct ParamTraits<Params, Params::Type> : public clapeze::EnumParam<FilterType> {
    ParamTraits()
        : clapeze::EnumParam<FilterType>("Type",
                                         "Type",
                                         {"SVF: Lowpass"
                                          "SVF: Highpass"
                                          "SVF: Bandpass"
                                          "Crossover: Low"
                                          "Crossover: High"
                                          "Biquad: Lowpass"},
                                         FilterType::SvfLowpass) {}
};

template <>
struct ParamTraits<Params, Params::Frequency> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Frequency", "Frequency", 20.0f, 20000.0f, 1000.0f) {
        mCurve = cPowCurve<2.0f>;
    }
};

template <>
struct ParamTraits<Params, Params::Q> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Q", "Q", 0.0f) {}
};

template <>
struct ParamTraits<Params, Params::Mix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Mix", "Mix", 1.0f) {}
};
}  // namespace clapeze::params

using namespace clapeze;

namespace filters {

class Processor : public EffectProcessor<ParamsFeature::AudioHandle> {
   public:
    explicit Processor(ParamsFeature::AudioHandle& params) : EffectProcessor(params) {}
    ~Processor() = default;

    ProcessStatus ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
        FilterType type = mParams.Get<Params::Type>();
        float cutoff = mParams.Get<Params::Frequency>();
        float Q = mParams.Get<Params::Q>();
        float mixf = mParams.Get<Params::Mix>();
        float sr = static_cast<float>(GetSampleRate());

        if (type != mType) {
            ProcessReset();
            mType = type;
        }

        // in
        switch (type) {
            case FilterType::SvfLowpass: {
                float r = Q * 0.89f;
                float filterSteepness = 0.5f;  // steeper means "achieves self-oscillation quicker"
                float filterQ = 0.5f * std::exp(filterSteepness * (r / (1 - r)));  // [0, 1] -> [0.5, inf]
                mSvf[0].SetFrequency(cutoff, sr, filterQ);
                mSvf[1].SetFrequency(cutoff, sr, filterQ);
                for (size_t idx = 0; idx < in.left.size(); ++idx) {
                    out.left[idx] = mSvf[0].Process<kitdsp::SvfFilterMode::LowPass>(out.left[idx]);
                    out.right[idx] = mSvf[1].Process<kitdsp::SvfFilterMode::LowPass>(out.right[idx]);
                }
            } break;
            case FilterType::SvfHighpass: {
                float r = Q * 0.89f;
                float filterSteepness = 0.5f;  // steeper means "achieves self-oscillation quicker"
                float filterQ = 0.5f * std::exp(filterSteepness * (r / (1 - r)));  // [0, 1] -> [0.5, inf]
                mSvf[0].SetFrequency(cutoff, sr, filterQ);
                mSvf[1].SetFrequency(cutoff, sr, filterQ);
                for (size_t idx = 0; idx < in.left.size(); ++idx) {
                    out.left[idx] = mSvf[0].Process<kitdsp::SvfFilterMode::Highpass>(out.left[idx]);
                    out.right[idx] = mSvf[1].Process<kitdsp::SvfFilterMode::Highpass>(out.right[idx]);
                }
            } break;
            case FilterType::SvfBandpass: {
                float r = Q * 0.89f;
                float filterSteepness = 0.5f;  // steeper means "achieves self-oscillation quicker"
                float filterQ = 0.5f * std::exp(filterSteepness * (r / (1 - r)));  // [0, 1] -> [0.5, inf]
                mSvf[0].SetFrequency(cutoff, sr, filterQ);
                mSvf[1].SetFrequency(cutoff, sr, filterQ);
                for (size_t idx = 0; idx < in.left.size(); ++idx) {
                    out.left[idx] = mSvf[0].Process<kitdsp::SvfFilterMode::BandPass>(out.left[idx]);
                    out.right[idx] = mSvf[1].Process<kitdsp::SvfFilterMode::BandPass>(out.right[idx]);
                }
            } break;
            case FilterType::CrossoverLow: {
                float low = 0.0f;
                float high = 0.0f;
                mCrossover[0].SetFrequency(cutoff, sr);
                mCrossover[1].SetFrequency(cutoff, sr);
                for (size_t idx = 0; idx < in.left.size(); ++idx) {
                    mCrossover[0].Process(out.left[idx], low, high);
                    out.left[idx] = low;
                    mCrossover[1].Process(out.right[idx], low, high);
                    out.right[idx] = low;
                }
            } break;
            case FilterType::CrossoverHigh: {
                float low = 0.0f;
                float high = 0.0f;
                mCrossover[0].SetFrequency(cutoff, sr);
                mCrossover[1].SetFrequency(cutoff, sr);
                for (size_t idx = 0; idx < in.left.size(); ++idx) {
                    mCrossover[0].Process(out.left[idx], low, high);
                    out.left[idx] = high;
                    mCrossover[1].Process(out.right[idx], low, high);
                    out.right[idx] = high;
                }
            } break;
            case FilterType::BiquadLowPass: {
                using namespace kitdsp::rbj;
                mBiquad[0].SetFrequency<BiquadFilterMode::LowPass>(cutoff, sr);
                mBiquad[0].SetQ<BiquadFilterMode::LowPass>(Q);
                mBiquad[1].SetFrequency<BiquadFilterMode::LowPass>(cutoff, sr);
                mBiquad[1].SetQ<BiquadFilterMode::LowPass>(Q);
                for (size_t idx = 0; idx < in.left.size(); ++idx) {
                    out.left[idx] = mBiquad[0].Process(out.left[idx]);
                    out.right[idx] = mBiquad[1].Process(out.right[idx]);
                }
            } break;
        }

        for (size_t idx = 0; idx < in.left.size(); ++idx) {
            out.left[idx] = kitdsp::lerp(in.left[idx], out.left[idx], mixf);
            out.right[idx] = kitdsp::lerp(in.right[idx], out.right[idx], mixf);
        }
        return ProcessStatus::Continue;
    }

    void ProcessReset() override {
        mSvf[0].Reset();
        mSvf[1].Reset();
        mCrossover[0].Reset();
        mCrossover[1].Reset();
        mBiquad[0].Reset();
        mBiquad[1].Reset();
    }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        (void)sampleRate;
        (void)minBlockSize;
        (void)maxBlockSize;
    }

   private:
    kitdsp::EmileSvf mSvf[2];
    kitdsp::CrossoverFilter mCrossover[2];
    kitdsp::rbj::BiquadFilter mBiquad[2];
    FilterType mType;
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, ParamsFeature& params) : kitgui::BaseApp(ctx), mParams(params) {}
    void OnUpdate() override {
        mParams.FlushFromAudio();
        ImGui::TextWrapped("Just directly exposing the filters my DSP library supports.");
        kitgui::DebugParam<ParamsFeature, Params::Type>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Frequency>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Q>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Mix>(mParams);
    }

   private:
    ParamsFeature& mParams;
};
#endif

class Plugin : public EffectPlugin {
   public:
    static const PluginEntry Entry;
    explicit Plugin(const clap_plugin_descriptor_t& meta) : EffectPlugin(meta) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        EffectPlugin::BaseConfig(false);

        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count)
                                    .Parameter<Params::Type>()
                                    .Parameter<Params::Frequency>()
                                    .Parameter<Params::Q>()
                                    .Parameter<Params::Mix>();
        ConfigFeature<TomlStateFeature<ParamsFeature>>(*this);

#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<clapeze::AssetsFeature>();
        ConfigFeature<KitguiFeature>(GetHost(),
                                     [&params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, params); });
#endif

        ConfigProcessor<Processor>(params.GetAudioHandle<ParamsFeature::AudioHandle>());
    }
};

CLAPEZE_REGISTER_PLUGIN(Plugin, AudioEffectDescriptor("kitsblips.filters", "KitsFilters", "Filter collection"));

}  // namespace filters
