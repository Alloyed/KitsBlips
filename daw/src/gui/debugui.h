#pragma once

#include <clap/ext/params.h>
#include <clapeze/params/baseParameter.h>
#include <clapeze/params/parameterTypes.h>
#include <string_view>
#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/controls/knob.h>
#include <kitgui/controls/toggleSwitch.h>

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

class BaseParamKnob : public kitgui::Knob {
   public:
    explicit BaseParamKnob(const clapeze::BaseParam& param, clap_id id, std::string_view sceneNode)
        : mParam(param), mId(id), mSceneNode(sceneNode) {
        // TODO: we'll need to listen for rescans if names can ever change
        mName = mParam.GetName();
        mMin = mParam.GetRawMin();
        mMax = mParam.GetRawMax();
        mIsStepped = (mParam.GetFlags() & CLAP_PARAM_IS_STEPPED) > 0;
    }
    ~BaseParamKnob() override = default;
    clap_id GetParamId() const { return mId; }
    const std::string& GetSceneNode() const { return mSceneNode; }
    bool IsStepped() const { return mIsStepped; }

   protected:
    const std::string& GetName() const override { return mName; }
    double GetDefault() const override { return mParam.GetRawDefault(); }
    std::string ToValueText(double value) const override {
        std::string buf(20, '\0');
        etl::span<char> span{buf.data(), buf.size()};
        mParam.ToText(value, span);
        buf = buf.c_str();
        return buf;
    }
    bool FromValueText(std::string_view text, double& value) const override { return mParam.FromText(text, value); }

   private:
    const clapeze::BaseParam& mParam;
    const clap_id mId;
    std::string mSceneNode;
    std::string mName{};
    bool mIsStepped;
};

class BaseParamToggle : public kitgui::ToggleSwitch {
   public:
    explicit BaseParamToggle(const clapeze::BaseParam& param, clap_id id, std::string_view sceneNode)
        : mParam(param), mId(id), mSceneNode(sceneNode) {
        // TODO: we'll need to listen for rescans if names can ever change
        mName = mParam.GetName();
    }
    ~BaseParamToggle() override = default;
    clap_id GetParamId() const { return mId; }
    const std::string& GetSceneNode() const { return mSceneNode; }

   protected:
    const std::string& GetName() const override { return mName; }
    bool GetDefault() const override { return mParam.GetRawDefault() > 0.5; }

   private:
    const clapeze::BaseParam& mParam;
    const clap_id mId;
    std::string mSceneNode;
    std::string mName{};
};
}  // namespace kitgui

#endif
