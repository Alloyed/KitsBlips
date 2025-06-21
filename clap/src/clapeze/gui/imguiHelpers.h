#pragma once
#ifdef KITSBLIPS_ENABLE_GUI

#include <imgui.h>
#include "clapeze/ext/parameters.h"

namespace ImGuiHelpers {
template <typename FN>
inline void beginMain(FN fn) {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    static bool open = true;
    ImGui::Begin("main", &open,
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);
    {
        fn();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

inline void displayParametersBasic(ParametersExt<clap_id>& params) {
    beginMain([&params]() {
        std::string buf;
        bool anyChanged = false;
        params.FlushFromAudio();
        for (clap_id id = 0; id < params.GetNumParams(); ++id) {
            const BaseParam* param = params.GetConfig(id);
            double raw = params.GetRaw(id);
            if (param->OnImgui(raw)) {
                params.SetRaw(id, raw);
                anyChanged = true;
            }
            if (ImGui::IsItemActivated()) {
                params.StartGesture(id);
            }
            if (ImGui::IsItemDeactivated()) {
                params.StopGesture(id);
            }
        }
        if (anyChanged) {
            params.RequestFlushIfNotProcessing();
        }
    });
}

}  // namespace ImGuiHelpers
#endif