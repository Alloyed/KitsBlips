#pragma once
#if KITSBLIPS_ENABLE_GUI
#include <clap/ext/params.h>
#include <clapeze/features/params/baseParameter.h>
#include <clapeze/features/params/parameterTypes.h>
#include <imgui.h>
#include <kitgui/controls/knob.h>
#include <kitgui/controls/toggleSwitch.h>

namespace kitgui {
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