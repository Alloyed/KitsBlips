#pragma once

#include <imgui.h>
#include <variant>
#include "clapApi/ext/parameters.h"

namespace ImGuiHelpers {
inline void displayParametersBasic(ParametersExt<clap_id>& params) {
    using P = ParametersExt<clap_id>;
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
            std::visit(
                [&](auto&& cfg) {
                    using T = std::decay_t<decltype(cfg)>;
                    if constexpr (std::is_same_v<T, P::NumericParam>) {
                        float value = static_cast<float>(params.Get(id));
                        buf = cfg.name;
                        if (ImGui::SliderFloat(buf.c_str(), &value, cfg.min, cfg.max)) {
                            params.Set(id, value);
                            anyChanged = true;
                        }
                        if (ImGui::IsItemActivated()) {
                            params.StartGesture(id);
                        }
                        if (ImGui::IsItemDeactivated()) {
                            params.StopGesture(id);
                        }
                    } else if constexpr (std::is_same_v<T, P::EnumParam>) {
                        size_t value = static_cast<size_t>(params.Get(id));
                        if (ImGui::BeginCombo(cfg.name.data(), cfg.labels[value].data())) {
                            for (size_t idx = 0; idx < cfg.labels.size(); ++idx) {
                                if (ImGui::Selectable(cfg.labels[idx].data(), idx == value)) {
                                    params.Set(id, static_cast<double>(idx));
                                    anyChanged = true;
                                }
                            }
                            ImGui::EndCombo();
                        }
                    } else {
                        static_assert(false, "non-exhaustive visitor!");
                    }
                },
                *params.GetConfig(id));
        }
        if (anyChanged) {
            params.RequestFlushIfNotProcessing();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
}  // namespace ImGuiHelpers