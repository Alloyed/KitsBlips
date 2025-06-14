#include <clap/clap.h>
#include <etl/queue_spsc_atomic.h>
#include <kitdsp/string.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include "clap/ext/params.h"
#include "clapeze/ext/parameters.h"
#include "imgui.h"
#include "kitdsp/math/util.h"

/**
 * Represents a numeric value. these are always mapped 0-1 on the DAW side so we can adjust the response curve to the
 * user's taste.
 */
class NumericParam : public BaseParam {
   public:
    using _valuetype = float;
    NumericParam(float mMin, float mMax, float mDefaultValue, std::string_view mName, std::string_view mUnit="")
        : mMin(mMin), mMax(mMax), mDefaultValue(mDefaultValue), mName(mName), mUnit(mUnit) {}
    bool FillInformation(clap_id id, clap_param_info_t* information) const override {
        memset(information, 0, sizeof(clap_param_info_t));
        information->id = id;
        information->flags = CLAP_PARAM_IS_AUTOMATABLE;
        information->min_value = 0.0;
        information->max_value = 1.0;
        information->default_value = mDefaultValue;
        kitdsp::stringCopy(information->name, mName);
        return true;
    }
    bool ToText(double rawValue, std::span<char>& outTextBuf) const override {
        float displayValue = 0.0f;
        if (!ToValue(rawValue, displayValue)) {
            return false;
        }
        if (mUnit.empty()) {
            snprintf(outTextBuf.data(), outTextBuf.size(), "%f", displayValue);
        } else {
            snprintf(outTextBuf.data(), outTextBuf.size(), "%f %s", displayValue, mUnit.data());
        }
        return true;
    }
    bool FromText(std::string_view text, double& outRawValue) const override {
        double in = std::strtod(text.data(), nullptr);
        return FromValue(in, outRawValue);
    }
    bool OnImgui(double& inOutRawValue) const override {
        float value = inOutRawValue;
        bool changed = ImGui::SliderFloat(mName.data(), &value, mMin, mMax);
        inOutRawValue = value;
        return changed;
    }
    bool ToValue(double rawValue, float& out) const {
        return kitdsp::lerpf(mMin, mMax, static_cast<float>(rawValue));
    }
    bool FromValue(float in, double& outRaw) const {
        float range = mMax - mMin;
        outRaw = range != 0.0f ? (in - mMin) / range : mMin;
        return true;
    }

   private:
    const float mMin;
    const float mMax;
    const float mDefaultValue;
    const std::string_view mName;
    const std::string_view mUnit;
};

/**
 * Represents an integer value. handy for managing the counts of things (eg. number of voices)
 */
class IntegerParam : public BaseParam {
   public:
    using _valuetype = int32_t;
    IntegerParam(int32_t mMin,
                 int32_t mMax,
                 int32_t mDefaultValue,
                 std::string_view mName,
                 std::string_view mUnit = "",
                 std::string_view mUnitSingular = "")
        : mMin(mMin),
          mMax(mMax),
          mDefaultValue(mDefaultValue),
          mName(mName),
          mUnit(mUnit),
          mUnitSingular(mUnitSingular) {}

    bool FillInformation(clap_id id, clap_param_info_t* information) const override {
        memset(information, 0, sizeof(clap_param_info_t));
        information->id = id;
        information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
        information->min_value = mMin;
        information->max_value = mMax;
        information->default_value = mDefaultValue;
        kitdsp::stringCopy(information->name, mName);
        return true;
    }
    bool ToText(double rawValue, std::span<char>& outTextBuf) const override {
        int32_t value = 0;
        if(!ToValue(rawValue, value))
        {
            return false;
        }
        if(!mUnit.empty())
        {
            std::string_view unit = mUnit;
            if(rawValue == 1.0 && !mUnitSingular.empty())
            {
                unit = mUnitSingular;
            }

            snprintf(outTextBuf.data(), outTextBuf.size(), "%d %s", value, unit.data());
        }
        else {
            snprintf(outTextBuf.data(), outTextBuf.size(), "%d", value);
        }
        return true;
    }
    bool FromText(std::string_view text, double& outRawValue) const override {
        int32_t parsed = static_cast<int32_t>(std::strtod(text.data(), nullptr));
        return FromValue(parsed, outRawValue);
    }
    bool OnImgui(double& inOutRawValue) const override {
        int32_t value = inOutRawValue;
        bool changed = ImGui::SliderInt(mName.data(), &value, mMin, mMax);
        inOutRawValue = value;
        return changed;
    }
    bool ToValue(double rawValue, int32_t& out) const {
        out = static_cast<int32_t>(rawValue);
        return true;
    }
    bool FromValue(int32_t in, double& outRaw) const {
        outRaw = static_cast<double>(in);
        return true;
    }

   private:
    const int32_t mMin;
    const int32_t mMax;
    const int32_t mDefaultValue;
    const std::string_view mName;
    const std::string_view mUnit;
    const std::string_view mUnitSingular;
};

/**
 * Represents a selection from a fixed set of options. A handy way to represent these is with dropdown values.
 */
template<typename EnumType>
class EnumParam : public BaseParam {
   public:
    using _valuetype = EnumType;
    EnumParam(std::vector<std::string_view> mLabels, std::string_view mName, EnumType mDefaultValue)
        : mLabels(std::move(mLabels)), mName(mName), mDefaultValue(mDefaultValue) {}
    bool FillInformation(clap_id id, clap_param_info_t* information) const override {
        double defaultRaw;
        FromValue(mDefaultValue, defaultRaw);

        memset(information, 0, sizeof(clap_param_info_t));
        information->id = id;
        information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_ENUM | CLAP_PARAM_IS_STEPPED;
        information->min_value = 0;
        information->max_value = static_cast<double>(mLabels.size());
        information->default_value = defaultRaw;
        kitdsp::stringCopy(information->name, mName);

        return true;
    }
    bool ToText(double rawValue, std::span<char>& outTextBuf) const override {
        size_t index = static_cast<size_t>(rawValue);
        if (index < mLabels.size()) {
            snprintf(outTextBuf.data(), outTextBuf.size(), "%s", mLabels[index].data());
        }
        return true;
    }
    bool FromText(std::string_view text, double& outRawValue) const override {
        for (size_t index = 0; index < mLabels.size(); ++index) {
            // TODO: trim whitespace, do case-insensitive compare, etc
            if (mLabels[index] == text) {
                outRawValue = static_cast<double>(index);
            }
        }
        return true;
    }
    bool OnImgui(double& inOutRawValue) const override {
        size_t selectedIndex = static_cast<size_t>(inOutRawValue);
        bool changed = false;
        if (ImGui::BeginCombo(mName.data(), mLabels[selectedIndex].data())) {
            for (size_t idx = 0; idx < mLabels.size(); ++idx) {
                if (ImGui::Selectable(mLabels[idx].data(), idx == selectedIndex)) {
                    FromValue(static_cast<EnumType>(idx), inOutRawValue);
                    changed = true;
                    //params.SetRaw(id, static_cast<double>(idx));
                    //anyChanged = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }
    bool ToValue(double rawValue, EnumType& out) const {
        size_t index = std::clamp<size_t>(static_cast<size_t>(rawValue), 0, mLabels.size());
        out = index;
        return true;
    }
    bool FromValue(EnumType in, double& outRaw) const {
        outRaw = static_cast<double>(in);
        return true;
    }

   private:
    const std::vector<std::string_view> mLabels;
    const std::string_view mName;
    const EnumType mDefaultValue;
};