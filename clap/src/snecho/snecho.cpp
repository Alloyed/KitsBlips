#include "snecho/snecho.h"

#include "clap/ext/timer-support.h"
#include "clapApi/ext/parameters.h"
#include "clapApi/ext/state.h"
#include "clapApi/ext/timerSupport.h"
#include "descriptor.h"
#include "gui/sdlImgui.h"
#include "imgui.h"

using namespace kitdsp;

const PluginEntry Snecho::Entry{
    AudioEffectDescriptor("kitsblips.snecho", "Snecho", "A SNES-inspired mono delay effect"),
    [](PluginHost& host) -> BasePlugin* { return new Snecho(host); }};

void Snecho::Config() {
    EffectPlugin::Config();
    ConfigExtension<SnechoParamsExt>(GetHost(), Params_Count)
        .configNumeric(Params_Size, 0.0f, 1.0f, 0.5f, "Size")
        .configNumeric(Params_Feedback, 0.0f, 1.0f, 0.5f, "Feedback")
        .configNumeric(Params_FilterPreset, 0.0f, 1.0f, 0.0f, "Filter Preset")
        .configNumeric(Params_SizeRange, 0.0f, 1.0f, 0.5f, "Size Range")
        .configNumeric(Params_Mix, 0.0f, 1.0f, 0.5f, "Mix")
        .configNumeric(Params_FreezeEcho, 0.0f, 1.0f, 0.0f, "Freeze Echo")
        .configNumeric(Params_EchoDelayMod, 0.0f, 1.0f, 1.0f, "Echo Mod")
        .configNumeric(Params_FilterMix, 0.0f, 1.0f, 0.5f, "Filter Mix")
        .configNumeric(Params_ClearBuffer, 0.0f, 1.0f, 0.0f, "Clear Buffer")
        .configNumeric(Params_ResetHead, 0.0f, 1.0f, 0.0f, "Reset Playhead");

    ConfigExtension<StateExt>();
    if(GetHost().SupportsExtension(CLAP_EXT_TIMER_SUPPORT))
    {
        ConfigExtension<TimerSupportExt>(GetHost());
    }
    if(GetHost().SupportsExtension(CLAP_EXT_GUI))
    {
        ConfigExtension<SdlImguiExt>(GetHost(), SdlImguiConfig{[this](){
                this->OnGui();
        }});
    }
}

void Snecho::OnGui() {
    SnechoParamsExt& params = SnechoParamsExt::GetFromPlugin<SnechoParamsExt>(*this);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    static bool open = true;
    ImGui::Begin("main", &open, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);
    {
        std::string buf;
        bool anyChanged = false;
        params.FlushFromAudio();
        for (clap_id id = Params_Size; id < Params_Count; ++id) {
            const auto cfg = params.GetConfig(id);
            float value = static_cast<float>(params.Get(id));
            buf = cfg->name;
            if(ImGui::SliderFloat(buf.c_str(), &value, cfg->min, cfg->max))
            {
                params.Set(id, value);
                anyChanged = true;
            }
            if(ImGui::IsItemActivated())
            {
                params.StartGesture(id);
            }
            if(ImGui::IsItemDeactivated())
            {
                params.StopGesture(id);
            }
        }
        if(anyChanged)
        {
            params.RequestFlushToHost();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void Snecho::ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out, SnechoParamsExt::AudioParameters& params) {
    // inputs
    // core
    snes1.cfg.echoBufferSize = params.Get(Params_Size);

    snes1.cfg.echoFeedback = params.Get(Params_Feedback);

    size_t filterPreset = static_cast<size_t>(params.Get(Params_FilterPreset) * SNES::kNumFilterPresets);
    if (filterPreset != mLastFilterPreset) {
        mLastFilterPreset = filterPreset;
        memcpy(snes1.cfg.filterCoefficients, SNES::kFilterPresets[filterPreset].data, SNES::kFIRTaps);
        // snes1.cfg.filterGain = dbToRatio(-SNES::kFilterPresets[filterPreset].maxGainDb);
    }

    size_t range = round(params.Get(Params_SizeRange));
    if (range == 0) {
        snes1.cfg.echoBufferRangeMaxSamples = SNES::kOriginalMaxEchoSamples;
    } else if (range == 1) {
        snes1.cfg.echoBufferRangeMaxSamples = SNES::kExtremeMaxEchoSamples;
    } else {
        snes1.cfg.echoBufferRangeMaxSamples = SNES::MsToSamples(10000.0f);
    }

    float wetDryMix = params.Get(Params_Mix);

    snes1.cfg.freezeEcho = (params.Get(Params_FreezeEcho)) > 0.5f;

    // extension
    snes1.cfg.echoDelayMod = params.Get(Params_EchoDelayMod);

    snes1.cfg.filterMix = params.Get(Params_FilterMix);

    snes1.mod.clearBuffer = params.Get(Params_ClearBuffer) > 0.5f;
    snes1.mod.resetHead = params.Get(Params_ResetHead) > 0.5f;
    snes1.cfg.echoBufferIncrementSamples = SNES::kOriginalEchoIncrementSamples;

    // processing
    for (size_t idx = 0; idx < in.left.size(); ++idx) {
        float drySignal = in.left[idx];
        float wetSignal = snesSampler.Process<kitdsp::interpolate::InterpolationStrategy::None>(
            drySignal, [this](float in, float& out) { out = snes1.Process(in * 0.5f) * 2.0f; });

        // outputs
        out.left[idx] = lerpf(drySignal, wetSignal, wetDryMix);
        out.right[idx] = lerpf(drySignal, wetSignal, wetDryMix);
    }
}

bool Snecho::Activate(double sampleRate, uint32_t minFramesCount, uint32_t maxFramesCount) {
    snesSampler = {SNES::kOriginalSampleRate, static_cast<float>(sampleRate)};
    return true;
}

void Snecho::Reset() {
    snes1.Reset();
}