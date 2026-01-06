#pragma once

#if KITSBLIPS_ENABLE_GUI
#include <clapeze/ext/parameterConfigs.h>
#include <imgui.h>

namespace kitgui {
inline bool DebugParam(const clapeze::NumericParam& param, double& inOutRawValue) {
    float value = 0.0f;
    param.ToValue(inOutRawValue, value);
    std::string label(param.mName);
    bool changed = ImGui::SliderFloat(label.c_str(), &value, param.mMin, param.mMax);
    param.FromValue(value, inOutRawValue);
    return changed;
}

inline bool DebugParam(const clapeze::IntegerParam& param, double& inOutRawValue) {
    int32_t value = 0.0f;
    param.ToValue(inOutRawValue, value);
    std::string label(param.mName);
    bool changed = ImGui::SliderInt(label.c_str(), &value, param.mMin, param.mMax);
    param.FromValue(value, inOutRawValue);
    return changed;
}

template <typename TEnum>
inline bool DebugParam(const clapeze::EnumParam<TEnum>& param, double& inOutRawValue) {
    TEnum value{};
    param.ToValue(inOutRawValue, value);
    size_t selectedIndex = static_cast<size_t>(value);
    bool changed = false;
    std::string label(param.mName);
    if (ImGui::BeginCombo(label.c_str(), param.mLabels[selectedIndex].data())) {
        for (size_t idx = 0; idx < param.mLabels.size(); ++idx) {
            std::string elementLabel(param.mLabels[idx]);
            if (ImGui::Selectable(elementLabel.c_str(), idx == selectedIndex)) {
                param.FromValue(static_cast<TEnum>(idx), inOutRawValue);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

template <typename TParamsFeature, TParamsFeature::Id id>
void DebugParam(TParamsFeature& feature) {
    double raw = feature.GetRawValue(id);

    if (DebugParam(*(feature.template GetSpecificParam<id>()), raw)) {
        feature.SetRawValue(id, raw);
    }
    if (ImGui::IsItemActivated()) {
        feature.StartGesture(id);
    }
    if (ImGui::IsItemDeactivated()) {
        feature.StopGesture(id);
    }
}
}  // namespace kitgui

#endif