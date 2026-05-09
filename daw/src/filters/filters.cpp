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
    CrossoverMixed,
    BiquadLowpass,
    BiquadHighpass,
    BiquadBandpass,
    BiquadAllpass,
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
                                         {"SVF: Lowpass",
                                          "SVF: HighPass",
                                          "SVF: Bandpass",
                                          "Crossover: Low",
                                          "Crossover: High",
                                          "Crossover: Mixed",
                                          "Biquad: Lowpass",
                                          "Biquad: Highpass",
                                          "Biquad: Bandpass",
                                          "Biquad: Allpass",
                                        },
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

        auto Setup =
            [&](auto expr) {
                expr(0);
                expr(1);
            };

        auto Filter =
            [&](auto expr) {
                for (size_t idx = 0; idx < in.left.size(); ++idx) {
                    out.left[idx] = kitdsp::lerp(in.left[idx], (expr)(0, in.left[idx]), mixf);
                }
                for (size_t idx = 0; idx < in.left.size(); ++idx) {
                    out.right[idx] = kitdsp::lerp(in.right[idx], (expr)(1, in.right[idx]), mixf);
                }
            };

        // in
        switch (type) {
            case FilterType::SvfLowpass: {
                float r = Q * 0.89f;
                float filterSteepness = 0.5f;  // steeper means "achieves self-oscillation quicker"
                float filterQ = 0.5f * std::exp(filterSteepness * (r / (1 - r)));  // [0, 1] -> [0.5, inf]
                Setup([&](auto i) { mSvf[i].SetFrequency(cutoff, sr, filterQ); });
                Filter([&](auto i, float s) { return mSvf[i].Process<kitdsp::SvfFilterMode::LowPass>(s); });
            } break;
            case FilterType::SvfHighpass: {
                float r = Q * 0.89f;
                float filterSteepness = 0.5f;  // steeper means "achieves self-oscillation quicker"
                float filterQ = 0.5f * std::exp(filterSteepness * (r / (1 - r)));  // [0, 1] -> [0.5, inf]
                Setup([&](auto i) { mSvf[i].SetFrequency(cutoff, sr, filterQ); });
                Filter([&](auto i, float s) { return mSvf[i].Process<kitdsp::SvfFilterMode::HighPass>(s); });
            } break;
            case FilterType::SvfBandpass: {
                float r = Q * 0.89f;
                float filterSteepness = 0.5f;  // steeper means "achieves self-oscillation quicker"
                float filterQ = 0.5f * std::exp(filterSteepness * (r / (1 - r)));  // [0, 1] -> [0.5, inf]
                Setup([&](auto i) { mSvf[i].SetFrequency(cutoff, sr, filterQ); });
                Filter([&](auto i, float s) { return mSvf[i].Process<kitdsp::SvfFilterMode::BandPass>(s); });
            } break;
            case FilterType::CrossoverLow: {
                float high = 0.0f;
                float low = 0.0f;
                Setup([&](auto i) { mCrossover[i].SetFrequency(cutoff, sr); });
                Filter([&](auto i, float s) {
                    mCrossover[i].Process(s, high, low);
                    return low;
                });
            } break;
            case FilterType::CrossoverHigh: {
                float high = 0.0f;
                float low = 0.0f;
                Setup([&](auto i) { mCrossover[i].SetFrequency(cutoff, sr); });
                Filter([&](auto i, float s) {
                    mCrossover[i].Process(s, high, low);
                    return high;
                });
            } break;
            case FilterType::CrossoverMixed: {
                float high = 0.0f;
                float low = 0.0f;
                Setup([&](auto i) { mCrossover[i].SetFrequency(cutoff, sr); });
                Filter([&](auto i, float s) {
                    mCrossover[i].Process(s, high, low);
                    return high+low;
                });
            } break;
            case FilterType::BiquadLowpass: {
                float mappedQ = kitdsp::lerp(0.8f, 10.0f, Q*Q*Q*Q);
                Setup([&](auto i) {
                    mBiquad[i].SetFrequency<kitdsp::rbj::BiquadFilterMode::LowPass>(cutoff, sr);
                    mBiquad[i].SetQ<kitdsp::rbj::BiquadFilterMode::LowPass>(mappedQ);
                 });
                Filter([&](auto i, float s) {
                    return mBiquad[i].Process(s);
                });
            } break;
            case FilterType::BiquadHighpass: {
                float mappedQ = kitdsp::lerp(0.03f, 72.0f, Q);
                Setup([&](auto i) {
                    mBiquad[i].SetFrequency<kitdsp::rbj::BiquadFilterMode::Highpass>(cutoff, sr);
                    mBiquad[i].SetQ<kitdsp::rbj::BiquadFilterMode::Highpass>(mappedQ);
                 });
                Filter([&](auto i, float s) {
                    return mBiquad[i].Process(s);
                });
            } break;
            case FilterType::BiquadBandpass: {
                float mappedQ = kitdsp::lerp(0.03f, 72.0f, Q);
                Setup([&](auto i) {
                    mBiquad[i].SetFrequency<kitdsp::rbj::BiquadFilterMode::BandPass>(cutoff, sr);
                    mBiquad[i].SetQ<kitdsp::rbj::BiquadFilterMode::BandPass>(mappedQ);
                 });
                Filter([&](auto i, float s) {
                    return mBiquad[i].Process(s);
                });
            } break;
            case FilterType::BiquadAllpass: {
                float mappedQ = kitdsp::lerp(0.03f, 72.0f, Q);
                Setup([&](auto i) {
                    mBiquad[i].SetFrequency<kitdsp::rbj::BiquadFilterMode::AllPass>(cutoff, sr);
                    mBiquad[i].SetQ<kitdsp::rbj::BiquadFilterMode::AllPass>(mappedQ);
                 });
                Filter([&](auto i, float s) {
                    return mBiquad[i].Process(s);
                });
            } break;
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
        EffectPlugin::BaseConfig(true);

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
