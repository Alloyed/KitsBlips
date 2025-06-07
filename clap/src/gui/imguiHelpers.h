#pragma once

#include "clapApi/ext/parameters.h"
#include "imgui.h"

namespace ImGuiHelpers {
inline void displayParametersBasic(ParametersExt<clap_id>& params) {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    static bool open = true;
    ImGui::Begin("main", &open, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);
    {
        std::string buf;
        bool anyChanged = false;
        params.FlushFromAudio();
        for (clap_id id = 0; id < params.GetNumParams(); ++id) {
            const auto cfg = params.GetConfig(id);
            float value = static_cast<float>(params.Get(id));
            buf = cfg->name;
            if (ImGui::SliderFloat(buf.c_str(), &value, cfg->min, cfg->max)) {
                params.Set(id, value);
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
            params.RequestFlushToHost();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
}  // namespace ImGuiHelpers