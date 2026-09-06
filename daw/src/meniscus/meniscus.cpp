#include <clapeze/effectPlugin.h>
#include <clapeze/entryPoint.h>
#include <clapeze/features/assetsFeature.h>
#include <clapeze/features/params/enumParametersFeature.h>
#include <clapeze/features/params/parameterTypes.h>
#include <clapeze/features/state/tomlStateFeature.h>
#include <clapeze/processor/baseProcessor.h>
#include <kitdsp/math/util.h>
#include <kitdsp/math/approx.h>
#include <kitdsp/control/lfo.h>
#include <kitdsp/sampling/delayLine.h>
#include <kitdsp/filters/onePole.h>
#include <kitdsp/math/units.h>
#include <kitdsp/osc/whiteNoise.h>
#include <kitdsp/util/spanAllocator.h>
#include <etl/vector.h>
#include "descriptor.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include "gui/kitguiFeature.h"
#endif

namespace {
enum class Params : clap_id {
    //primary
    Mix,
    Tone,
    PanSpeed,
    GrainDensity,
    GrainLength,
    GrainPitch,
    PanStrategy,
    Bypass,
    BufferFreeze,
    //alt
    ReverbMix,
    ModDepth,
    ModSpeed,
    ReverbLength,
    BufferLength,
    GrainPitchOdds,
    StereoWidth,
    Count
};
enum class PanStrategy {
    Zen,
    Swirl,
    Torrent,
};
enum class PitchRange {
    None,
    Minus1,
    Plus1,
    Minus1Plus1,
    Minus1Plus2,
    Minus2Plus1,
    Minus2Plus2,
};
using ParamsFeature = clapeze::params::EnumParametersFeature<Params>;
}  // namespace

namespace clapeze::params {
template <>
struct ParamTraits<Params, Params::Mix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Mix", "Mix", 1.0f) {}
};
template <>
struct ParamTraits<Params, Params::Tone> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Tone", "Tone", 1.0f) {}
};
template<>
struct ParamTraits<Params, Params::PanSpeed> : public clapeze::NumericParam {
    ParamTraits()
        : clapeze::NumericParam("PanSpeed", "Speed", 0.001f, 20.0f, 0.2f, "hz") {
        mCurve = clapeze::cPowCurve<3.0f>;
    }
};
template <>
struct ParamTraits<Params, Params::GrainDensity> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("GrainDensity", "Density", 1.0f) {}
};
template <>
struct ParamTraits<Params, Params::GrainLength> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("GrainLength", "Length", 1.0f) {}
};
template <>
struct ParamTraits<Params, Params::GrainPitch> : public clapeze::EnumParam<PitchRange> {
    ParamTraits()
        : clapeze::EnumParam<PitchRange>("GrainPitch",
                                            "Pitch",
                                            {"0", "-1", "+1", "-1/+1", "-1/+2", "-2/+1", "-2/+2"},
                                            PitchRange::None) {}
};
template <>
struct ParamTraits<Params, Params::PanStrategy> : public clapeze::EnumParam<PanStrategy> {
    ParamTraits()
        : clapeze::EnumParam<PanStrategy>("PanStrategy",
                                            "Z|S|T",
                                            {"Zen", "Swirl", "Torrent"},
                                            PanStrategy::Zen) {}
};
template <>
struct ParamTraits<Params, Params::Bypass> : public clapeze::OnOffParam {
    ParamTraits() : clapeze::OnOffParam("Bypass", "Bypass", OnOff::Off) {}
};
template <>
struct ParamTraits<Params, Params::BufferFreeze> : public clapeze::OnOffParam {
    ParamTraits() : clapeze::OnOffParam("BufferFreeze", "Slush", OnOff::Off) {}
};
template <>
struct ParamTraits<Params, Params::ReverbMix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Reverb Mix", "Reverb", 1.0f) {}
};
template <>
struct ParamTraits<Params, Params::ModDepth> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("ModDepth", "Depth", 1.0f) {}
};
template<>
struct ParamTraits<Params, Params::ModSpeed> : public clapeze::NumericParam {
    ParamTraits()
        : clapeze::NumericParam("ModSpeed", "Rate", 0.001f, 20.0f, 0.2f, "hz") {
        mCurve = clapeze::cPowCurve<3.0f>;
    }
};
template <>
struct ParamTraits<Params, Params::ReverbLength> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("ReverbLength", "Decay", 1.0f, 1000.0f, 1.0f, "ms") {
        mCurve = cPowCurve<2.0f>;
    }
};
template <>
struct ParamTraits<Params, Params::BufferLength> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("BufferLength", "Length", 200.0f, 100000.0f, 5000.0f, "ms") {
        mCurve = cPowCurve<2.0f>;
    }
};
template <>
struct ParamTraits<Params, Params::GrainPitchOdds> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("GrainPitchOdds", "Probability", .1f, 1.0f, 0.1f, "") {}
};
template <>
struct ParamTraits<Params, Params::StereoWidth> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("StereoWidth", "Width", .5f, 1.0f, 0.5f, "") {}
};
}  // namespace clapeze::params

using namespace clapeze;

namespace meniscus {
using namespace kitdsp;

// TODO : this was too lazy, time to find a nice shelf filter
class ToneFilter {
   public:
    explicit ToneFilter(float sampleRate) { mPole1.SetFrequency(1200.0f, sampleRate); }
    float Process(float in, float tone) {
        float lowpass = mPole1.Process(in);
        float highpass = in - lowpass;

        // needs to be 2 at 0.5 and 1 at 0, 1, in between is a matter of taste
        float gain = sinf(tone * kitdsp::kPi) + 1.0f;

        return kitdsp::lerp(lowpass, highpass, tone) * gain;
    }
    void Reset() { mPole1.Reset(); }
    kitdsp::OnePole mPole1;
};

float hanningWindow(float t) {
    if (t <= 0.0f || t >= 1.0f) {
        return 0.0f;
    }
    return 0.5f * (1.0f - kitdsp::approx::cos2pif_nasty(t));
}

float rectWindow(float t) {
    if (t <= 0.0f || t >= 1.0f) {
        return 0.0f;
    }
    return 1.0f;
}

struct Grain {
    float sizeSamples = 0.0f;
    float samplesPlayed = 0.0f;
    float speed = 0.0f;

    void Set(float start, float size, float speed) {
        this->pos = start;
        this->sizeSamples = size;
        this->speed = speed;
        this->samplesPlayed = 0.0f;
    }
    void Advance(bool frozen) {
        if (frozen) {
            pos = std::max(0.0f, pos - speed);
        } else {
            pos = std::max(0.0f, pos + 1.0f - speed);
        }
        samplesPlayed += speed;
    }
    float Read(DelayLine<float>& buf) const {
        if (sizeSamples == 0.0f) {
            return 0.0f;
        }
        float progress = kitdsp::clamp(samplesPlayed / sizeSamples, 0.0f, 1.0f);
        using namespace kitdsp::interpolate;
        return buf.Read<InterpolationStrategy::Linear>(pos) * hanningWindow(progress);
    }
    bool Finished() const {
        if (sizeSamples == 0.0f) {
            return true;
        }
        float progress = kitdsp::clamp(samplesPlayed / sizeSamples, 0.0f, 1.0f);
        return progress == 1.0f;
    }

   private:
    float pos;
};

struct Dsp {
    static constexpr double kMaxSeconds = 15.0;
    Dsp(double sampleRate, DynamicSpanAllocator<float>& memory):
        mDelay(memory.alloc(narrow_cast<size_t>(sampleRate * kMaxSeconds))),
        mTone(sampleRate)
    {}
    void Reset() {
        mTone.Reset();
        mDelay.Reset();
        mGrains.clear();
        mGrainClock.Reset();
        mInitialPan.Reset();
        mNoise.Reset();
    }
    kitdsp::DelayLine<float> mDelay;
    ToneFilter mTone;
    etl::vector<Grain, 32> mGrains{};
    kitdsp::lfo::ImpulseTrain mGrainClock{};
    kitdsp::lfo::SineOscillator mInitialPan{};
    kitdsp::WhiteNoise mNoise{};
};

class Processor : public EffectProcessor<ParamsFeature::AudioHandle> {
   public:
    explicit Processor(clapeze::PluginHost& host,ParamsFeature::AudioHandle& params) : EffectProcessor(host,params) {}
    ~Processor() = default;

    ProcessStatus ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
        mDsp->mGrainClock.SetFrequency(kitdsp::lerp(0.2, 20.0f, mParams.Get<Params::GrainDensity>()), GetSampleRate());
        if(mDsp->mGrainClock.Process(in.left.size())) {
            float speed = 1.0f;
            float odds = mParams.Get<Params::GrainPitchOdds>();
            PitchRange range = mParams.Get<Params::GrainPitch>();
            if(mDsp->mNoise.ProcessNormalized() <= odds) {
                switch(range) {
                    case PitchRange::None:
                        break;
                    case PitchRange::Minus1:
                        speed = 0.5f;
                        break;
                    case PitchRange::Plus1:
                        speed = 2.0f;
                        break;
                    case PitchRange::Minus1Plus1:
                        speed = std::exp2(narrow_cast<float>(mDsp->mNoise.ProcessInt(-1, 1)));
                        break;
                    case PitchRange::Minus1Plus2:
                        speed = std::exp2(narrow_cast<float>(mDsp->mNoise.ProcessInt(-1, 2)));
                        break;
                    case PitchRange::Minus2Plus1:
                        speed = std::exp2(narrow_cast<float>(mDsp->mNoise.ProcessInt(-2, 1)));
                        break;
                    case PitchRange::Minus2Plus2:
                        speed = std::exp2(narrow_cast<float>(mDsp->mNoise.ProcessInt(-2, 2)));
                        break;
                }
            }

            float lengthSamples = kitdsp::lerp(100.0f, 3000.0f, mParams.Get<Params::GrainLength>())
            // *8 ensures grains will always be long enough, even if they are played 2 octaves up (*8 speed)
            float delaySamples = lengthSamples * 8.0f + 1.0f;

            Grain& g = mDsp->mGrains.emplace_back();
            g.Set(delaySamples, lengthSamples, speed);
        }

        float mixf = mParams.Get<Params::Mix>();
        float tone = mParams.Get<Params::Tone>();
        bool bufferFreeze = mParams.Get<Params::BufferFreeze>();
        for (size_t idx = 0; idx < in.left.size(); ++idx) {
            // in
            float left = in.left[idx];
            float right = in.right[idx];
            float mono = mDsp->mTone.Process(kitdsp::lerp(left, right, 0.5f), tone);
            if(!bufferFreeze) {
                mDsp->mDelay.Write(mono);
            }

            float processedLeft = 0.0f;
            float processedRight = 0.0f;
            for(auto& grain : mDsp->mGrains) {
                grain.Advance(bufferFreeze);
                processedLeft += grain.Read(mDsp->mDelay);
                processedRight += grain.Read(mDsp->mDelay);
            }

            // outputs
            out.left[idx] = kitdsp::lerp(left, processedLeft, mixf);
            out.right[idx] = kitdsp::lerp(right, processedRight, mixf);
        }

        etl::erase_if(mDsp->mGrains, [] (const auto& g) { return g.Finished(); });

        return ProcessStatus::Continue;
    }

    void ProcessReset() override {
        mDsp->Reset();
    }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        (void)minBlockSize;
        (void)maxBlockSize;
        mMemory.reset();
        mDsp = std::make_unique<Dsp>(sampleRate, mMemory);
    }

   private:
    kitdsp::DynamicSpanAllocator<float> mMemory{};
    std::unique_ptr<Dsp> mDsp;
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, ParamsFeature& params) : kitgui::BaseApp(ctx), mParams(params) {}
    void OnUpdate() override {
        mParams.FlushFromAudio();
        ImGui::TextWrapped("UI meniscus (TODO)");
        /*mParams.DebugImGui();*/
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
            .Parameter<Params::Mix>()
            .Parameter<Params::Tone>()
            .Parameter<Params::PanSpeed>()
            .Parameter<Params::GrainDensity>()
            .Parameter<Params::GrainLength>()
            .Parameter<Params::GrainPitch>()
            .Parameter<Params::PanStrategy>()
            .Parameter<Params::Bypass>()
            .Parameter<Params::BufferFreeze>()
            .Parameter<Params::ReverbMix>()
            .Parameter<Params::ModDepth>()
            .Parameter<Params::ModSpeed>()
            .Parameter<Params::ReverbLength>()
            .Parameter<Params::BufferLength>()
            .Parameter<Params::GrainPitchOdds>()
            .Parameter<Params::StereoWidth>();
        ConfigFeature<TomlStateFeature<ParamsFeature>>(*this);

#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<clapeze::AssetsFeature>();
        ConfigFeature<KitguiFeature>(GetHost(),
                                     [&params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, params); });
#endif

        ConfigProcessor<Processor>(params.GetAudioHandle<ParamsFeature::AudioHandle>());
    }
};

CLAPEZE_REGISTER_PLUGIN(Plugin, AudioEffectDescriptor("kitsblips.meniscus", "meniscus", "Plugin description"));

}  // namespace meniscus
