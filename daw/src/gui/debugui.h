#pragma once

#if KITSBLIPS_ENABLE_GUI

#include <clap/ext/params.h>
#include <clapeze/features/params/baseParameter.h>
#include <clapeze/features/params/parameterTypes.h>
#include <imgui.h>
#include <kitgui/controls/knob.h>
#include <kitgui/controls/toggleSwitch.h>

/**
 * TODO: consider rewriting this to be non-specific and instead work off of only what BaseParam provides (this is what
 * DAW's do, after all!)
 */
namespace kitgui {
inline bool DebugParam(const clapeze::NumericParam& param, double& inOutRawValue) {
    // We can't use ToValue/FromValue versions; those have nonlinear curves
    float value = static_cast<float>(inOutRawValue);
    std::string label(param.mName);
    char buf[64] = {};
    etl::span<char> span{buf, 64};
    param.ToText(inOutRawValue, span);
    bool changed = ImGui::SliderFloat(label.c_str(), &value, 0.0f, 1.0f, buf,
                                      ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoRoundToFormat);
    inOutRawValue = value;
    return changed;
}

inline bool DebugParam(const clapeze::IntegerParam& param, double& inOutRawValue) {
    int32_t value = 0;
    param.ToValue(inOutRawValue, value);
    std::string label(param.mName);
    bool changed =
        ImGui::SliderInt(label.c_str(), &value, param.mMin, param.mMax, nullptr, ImGuiSliderFlags_AlwaysClamp);
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
    auto& handle = feature.GetMainHandle();
    auto index = static_cast<clap_id>(id);
    double raw = handle.GetRawValue(index);

    if (DebugParam(*(feature.template GetSpecificParam<id>()), raw)) {
        handle.SetRawValue(index, raw);
    }
    if (ImGui::IsItemActivated()) {
        handle.StartGesture(index);
    }
    if (ImGui::IsItemDeactivated()) {
        handle.StopGesture(index);
    }
}
}  // namespace kitgui

#endif
