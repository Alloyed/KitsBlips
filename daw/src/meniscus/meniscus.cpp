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
#include <kitdsp/math/vector.h>
#include <kitdsp/volume/panning.h>
#include <kitdsp/osc/whiteNoise.h>
#include <kitdsp/util/spanAllocator.h>
#include <etl/vector.h>
#include "descriptor.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include "gui/debugui.h"
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
    ParamTraits() : clapeze::PercentParam("Mix", "Mix", 0.5f) {}
};
template <>
struct ParamTraits<Params, Params::Tone> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Tone", "Tone", 0.5f) {}
};
template<>
struct ParamTraits<Params, Params::PanSpeed> : public clapeze::NumericParam {
    ParamTraits()
        : clapeze::NumericParam("PanSpeed", "Speed", 0.001f, 20.0f, 0.2f, "hz") {
        mCurve = clapeze::cPowCurve<3.0f>;
    }
};
template<>
struct ParamTraits<Params, Params::GrainDensity> : public clapeze::NumericParam {
    ParamTraits()
        : clapeze::NumericParam("GrainDensity", "Density", 0.02f, 20.0f, 6.0f, "hz") {
        mCurve = clapeze::cPowCurve<2.0f>;
    }
};
template <>
struct ParamTraits<Params, Params::GrainLength> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("GrainLength", "Length", 40.0f, 400.0f, 40.0f, "ms") {
        mCurve = clapeze::cPowCurve<2.0f>;
    }
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
    ParamTraits() : clapeze::PercentParam("Reverb Mix", "Reverb", 0.0f) {}
};
template <>
struct ParamTraits<Params, Params::ModDepth> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("ModDepth", "Depth", 0.0f) {}
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
    ParamTraits() : clapeze::NumericParam("BufferLength", "Length", 2000.0f, 10000.0f, 5000.0f, "ms") {
        mCurve = cPowCurve<2.0f>;
    }
};
template <>
struct ParamTraits<Params, Params::GrainPitchOdds> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("GrainPitchOdds", "Probability", .1f, 1.0f, 0.1f, "") {}
};
template <>
struct ParamTraits<Params, Params::StereoWidth> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("StereoWidth", "Width", .5f, 1.0f, 1.0f, "") {}
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

float chunkyWindow(float t) {
    if (t <= 0.0f || t >= 1.0f) {
        return 0.0f;
    }
    return kitdsp::clamp(0.75f * (1.0f - kitdsp::approx::cos2pif_nasty(t)), 0.0f, 1.0f);
}

float rectWindow(float t) {
    if (t <= 0.0f || t >= 1.0f) {
        return 0.0f;
    }
    return 1.0f;
}

class Grain {
    public:
    void Set(float start, float size, float speed, float pan) {
        this->pos = start;
        this->sizeSamples = size;
        this->speed = speed;
        this->pan = kitdsp::equalPowerPan(pan);
        this->samplesPlayed = 0.0f;
    }
    void Advance() {
        pos = std::max(0.0f, pos + 1.0f - speed);
        samplesPlayed += speed;
    }
    kitdsp::float_2 Read(DelayLine<float_2>& buf) const {
        if (sizeSamples == 0.0f) {
            return {};
        }
        float progress = kitdsp::clamp(samplesPlayed / sizeSamples, 0.0f, 1.0f);
        using namespace kitdsp::interpolate;
        float_2 out = buf.Read<InterpolationStrategy::Hermite>(pos) * hanningWindow(progress);
        return out * pan;
    }
    bool Finished() const {
        if (sizeSamples == 0.0f) {
            return true;
        }
        float progress = kitdsp::clamp(samplesPlayed / sizeSamples, 0.0f, 1.0f);
        return progress == 1.0f;
    }

   private:
    float sizeSamples = 0.0f;
    float samplesPlayed = 0.0f;
    float speed = 0.0f;
    float_2 pan = {};
    float pos;
};

struct Dsp {
    static constexpr double kMaxSeconds = 10.0;
    Dsp(float sampleRate, DynamicSpanAllocator<float_2>& memory):
        mDelay(memory.alloc(narrow_cast<size_t>(sampleRate * kMaxSeconds))),
        mToneL(sampleRate),
        mToneR(sampleRate)
    {}
    void Reset() {
        mToneL.Reset();
        mToneR.Reset();
        mDelay.Reset();
        mGrains.clear();
        mGrainClock.Reset();
        mInitialPan.Reset();
        mNoise.Reset();
    }
    kitdsp::DelayLine<float_2> mDelay;
    ToneFilter mToneL;
    ToneFilter mToneR;
    etl::vector<Grain, 32> mGrains{};
    kitdsp::lfo::ImpulseTrain mGrainClock{};
    kitdsp::lfo::Phasor mInitialPan{};
    kitdsp::WhiteNoise mNoise{};
};

class Processor : public EffectProcessor<ParamsFeature::AudioHandle> {
   public:
    explicit Processor(clapeze::PluginHost& host,ParamsFeature::AudioHandle& params) : EffectProcessor(host,params) {}
    ~Processor() = default;

    ProcessStatus ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
        float sampleRate = static_cast<float>(GetSampleRate());
        float numSamples = static_cast<float>(in.left.size());
        PanStrategy strategy = mParams.Get<Params::PanStrategy>();
        mDsp->mInitialPan.SetFrequency(mParams.Get<Params::PanSpeed>() + mSpeedOffset, sampleRate);
        mDsp->mGrainClock.SetFrequency(mParams.Get<Params::GrainDensity>(), sampleRate);
        mDsp->mDelay.SetSize(kitdsp::msToSamples(mParams.Get<Params::BufferLength>(), sampleRate));

        mDsp->mInitialPan.Process(numSamples);
        if(mDsp->mGrainClock.Process(numSamples)) {
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
                    case PitchRange::Minus1Plus1: {
                        static constexpr std::array kChoices = {-1.0f, 1.0f};
                        speed = std::exp2(kChoices[mDsp->mNoise.ProcessIndex(kChoices.size())]);
                        break;
                    }
                    case PitchRange::Minus1Plus2:{
                        static constexpr std::array kChoices = {-1.0f, 1.0f, 2.0f};
                        speed = std::exp2(kChoices[mDsp->mNoise.ProcessIndex(kChoices.size())]);
                        break;
                    }
                    case PitchRange::Minus2Plus1: {
                        static constexpr std::array kChoices = {-2.0f, -1.0f, 1.0f};
                        speed = std::exp2(kChoices[mDsp->mNoise.ProcessIndex(kChoices.size())]);
                        break;
                    }
                    case PitchRange::Minus2Plus2: {
                        static constexpr std::array kChoices = {-2.0f, -1.0f, 1.0f, 2.0f};
                        speed = std::exp2(kChoices[mDsp->mNoise.ProcessIndex(kChoices.size())]);
                        break;
                    }
                }
            }

            float lengthSamples = kitdsp::msToSamples(mParams.Get<Params::GrainLength>(), sampleRate);
            float delaySamples = lengthSamples * (speed+0.25f) + 1.0f;
            float pan{};
            switch(strategy) {
                case PanStrategy::Zen:
                   pan = kitdsp::approx::cos2pif_nasty(mDsp->mInitialPan.GetPhase());
                   mSpeedOffset = 0;
                   break;
                case PanStrategy::Swirl:
                   pan = kitdsp::approx::tanh(kitdsp::approx::cos2pif_nasty(mDsp->mInitialPan.GetPhase()) * 3.0f);
                   mSpeedOffset = 0;
                   break;
                case PanStrategy::Torrent:
                   pan = kitdsp::approx::tanh(kitdsp::approx::cos2pif_nasty(mDsp->mInitialPan.GetPhase()) * 3.0f);
                   mSpeedOffset = mDsp->mNoise.Process() * 0.25;
                   break;
            }
            pan = (pan * mParams.Get<Params::StereoWidth>() / 2) + 0.5f;

            Grain& g = mDsp->mGrains.emplace_back();
            g.Set(delaySamples, lengthSamples, speed, pan);
        }

        float mixf = mParams.Get<Params::Mix>();
        if(mixf < 0.001 || mParams.Get<Params::Bypass>() == clapeze::OnOff::On) {
            out.CopyFrom(in);
            return ProcessStatus::Continue;
        }

        float tone = mParams.Get<Params::Tone>();
        bool bufferFreeze = mParams.Get<Params::BufferFreeze>() == clapeze::OnOff::On;
        for (size_t idx = 0; idx < in.left.size(); ++idx) {
            // in
            float left = in.left[idx];
            float right = in.right[idx];
            if(bufferFreeze) {
                mDsp->mDelay.AdvanceFrozen();
            } else {
                mDsp->mDelay.Write({
                    mDsp->mToneL.Process(left, tone),
                    mDsp->mToneR.Process(right, tone)
                });
            }

            float processedLeft = 0.0f;
            float processedRight = 0.0f;
            for(auto& grain : mDsp->mGrains) {
                grain.Advance();
                kitdsp::float_2 out = grain.Read(mDsp->mDelay);
                processedLeft += out.left;
                processedRight += out.right;
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
        mSpeedOffset = 0.0f;
    }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        (void)minBlockSize;
        (void)maxBlockSize;
        mMemory.reset();
        mDsp = std::make_unique<Dsp>(static_cast<float>(sampleRate), mMemory);
    }

   private:
    kitdsp::DynamicSpanAllocator<float_2> mMemory{};
    float mSpeedOffset = 0.0f;
    std::unique_ptr<Dsp> mDsp;
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, ParamsFeature& params) : kitgui::BaseApp(ctx), mParams(params) {}
    void OnUpdate() override {
        mParams.FlushFromAudio();
        ImGui::TextWrapped("temp ui :) swag");
        ImGui::TextWrapped("https://bsky.app/profile/hyenablood.yeen.world/post/3mutleh4fgs2v");
        if(!mAlt) {
            kitgui::DebugParam<ParamsFeature, Params::Mix>(mParams);
            kitgui::DebugParam<ParamsFeature, Params::Tone>(mParams);
            kitgui::DebugParam<ParamsFeature, Params::PanSpeed>(mParams);
            kitgui::DebugParam<ParamsFeature, Params::GrainDensity>(mParams);
            kitgui::DebugParam<ParamsFeature, Params::GrainLength>(mParams);
            kitgui::DebugParam<ParamsFeature, Params::GrainPitch>(mParams);
            kitgui::DebugParam<ParamsFeature, Params::PanStrategy>(mParams);
        } else {
            kitgui::DebugParam<ParamsFeature, Params::ReverbMix>(mParams);
            kitgui::DebugParam<ParamsFeature, Params::ModDepth>(mParams);
            kitgui::DebugParam<ParamsFeature, Params::ModSpeed>(mParams);
            kitgui::DebugParam<ParamsFeature, Params::ReverbLength>(mParams);
            kitgui::DebugParam<ParamsFeature, Params::BufferLength>(mParams);
            kitgui::DebugParam<ParamsFeature, Params::GrainPitchOdds>(mParams);
            kitgui::DebugParam<ParamsFeature, Params::StereoWidth>(mParams);
        }
        kitgui::DebugParam<ParamsFeature, Params::Bypass>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::BufferFreeze>(mParams);
        ImGui::Checkbox("Alt", &mAlt);
    }

   private:
    ParamsFeature& mParams;
    bool mAlt = false;
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

CLAPEZE_REGISTER_PLUGIN(Plugin, AudioEffectDescriptor("kitsblips.meniscus", "Meniscus", "Plugin description"));

}  // namespace meniscus
