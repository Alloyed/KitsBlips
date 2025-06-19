#pragma once

#include <clap/clap.h>
#include <clap/ext/params.h>
#include <etl/span.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

#ifdef KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#endif

#include "clapeze/ext/parameters.h"

namespace _clapeze_impl {
/* safe alternative to strcpy when the buffer size is known at compile time. assumes the buffer is zero'd out in
 * advance*/
template <size_t BUFFER_SIZE>
void stringCopy(char (&buffer)[BUFFER_SIZE], std::string_view src) {
    static_assert(BUFFER_SIZE > 0);
    // copy at most BUFFER_SIZE-1 bytes. the last byte is reserved for the null terminator
    std::memcpy(buffer, src.data(), std::min(src.length(), BUFFER_SIZE - 1));
}
}  // namespace _clapeze_impl

/**
 * Represents a numeric value. these are always mapped 0-1 on the DAW side so we can adjust the response curve to the
 * user's taste.
 */
class NumericParam : public BaseParam {
   public:
    using _valuetype = float;
    NumericParam(std::string_view mModule,
                 std::string_view mName,
                 float mMin,
                 float mMax,
                 float mDefaultValue,
                 std::string_view mUnit = "")
        : mModule(mModule), mName(mName), mMin(mMin), mMax(mMax), mDefaultValue(mDefaultValue), mUnit(mUnit) {}
    bool FillInformation(clap_id id, clap_param_info_t* information) const override {
        memset(information, 0, sizeof(clap_param_info_t));
        information->id = id;
        information->flags = CLAP_PARAM_IS_AUTOMATABLE;
        information->min_value = 0.0;
        information->max_value = 1.0;
        information->default_value = GetRawDefault();
        _clapeze_impl::stringCopy(information->name, mName);
        _clapeze_impl::stringCopy(information->module, mModule);
        return true;
    }
    double GetRawDefault() const override {
        double rawDefault;
        assert(FromValue(mDefaultValue, rawDefault));
        return rawDefault;
    }
    bool ToText(double rawValue, etl::span<char>& outTextBuf) const override {
        float displayValue = 0.0f;
        if (!ToValue(rawValue, displayValue)) {
            return false;
        }
        if (mUnit.empty()) {
            snprintf(outTextBuf.data(), outTextBuf.size(), "%.3f", displayValue);
        } else {
            snprintf(outTextBuf.data(), outTextBuf.size(), "%.3f %s", displayValue, mUnit.data());
        }
        return true;
    }
    bool FromText(std::string_view text, double& outRawValue) const override {
        double in = std::strtod(text.data(), nullptr);
        return FromValue(in, outRawValue);
    }
#ifdef KITSBLIPS_ENABLE_GUI
    bool OnImgui(double& inOutRawValue) const override {
        float value = 0.0f;
        ToValue(inOutRawValue, value);
        bool changed = ImGui::SliderFloat(mName.data(), &value, mMin, mMax);
        FromValue(value, inOutRawValue);
        return changed;
    }
#endif
    bool ToValue(double rawValue, float& out) const {
        out = std::lerp(mMin, mMax, static_cast<float>(rawValue));
        return true;
    }
    bool FromValue(float in, double& outRaw) const {
        float range = mMax - mMin;
        outRaw = range != 0.0f ? (in - mMin) / range : mMin;
        return true;
    }

   private:
    const std::string_view mModule;
    const std::string_view mName;
    const float mMin;
    const float mMax;
    const float mDefaultValue;
    const std::string_view mUnit;
};

class PercentParam : public NumericParam {
   public:
    using _valuetype = float;
    PercentParam(std::string_view mModule, std::string_view mName, float mDefaultValue)
        : NumericParam(mModule, mName, 0.0f, 1.0f, mDefaultValue) {}
    bool ToText(double rawValue, etl::span<char>& outTextBuf) const override {
        float displayValue = rawValue * 100.0f;
        snprintf(outTextBuf.data(), outTextBuf.size(), "%.2f%%", displayValue);
        return true;
    }
    bool FromText(std::string_view text, double& outRawValue) const override {
        double in = std::strtod(text.data(), nullptr);
        return FromValue(in / 100.0f, outRawValue);
    }
};

class DbParam : public NumericParam {
   public:
    DbParam(std::string_view module, std::string_view name, float minValue, float maxValue, float mDefaultValue)
        : NumericParam(module, name, minValue, maxValue, mDefaultValue, "db") {}
};

/**
 * Represents an integer value. handy for managing the counts of things (eg. number of voices)
 */
class IntegerParam : public BaseParam {
   public:
    using _valuetype = int32_t;
    IntegerParam(std::string_view mModule,
                 std::string_view mName,
                 int32_t mMin,
                 int32_t mMax,
                 int32_t mDefaultValue,
                 std::string_view mUnit = "",
                 std::string_view mUnitSingular = "")
        : mModule(mModule),
          mName(mName),
          mMin(mMin),
          mMax(mMax),
          mDefaultValue(mDefaultValue),
          mUnit(mUnit),
          mUnitSingular(mUnitSingular) {}

    bool FillInformation(clap_id id, clap_param_info_t* information) const override {
        memset(information, 0, sizeof(clap_param_info_t));
        information->id = id;
        information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
        information->min_value = static_cast<double>(mMin);
        information->max_value = static_cast<double>(mMax);
        information->default_value = GetRawDefault();
        _clapeze_impl::stringCopy(information->name, mName);
        return true;
    }
    double GetRawDefault() const override {
        double rawDefault;
        assert(FromValue(mDefaultValue, rawDefault));
        return rawDefault;
    }
    bool ToText(double rawValue, etl::span<char>& outTextBuf) const override {
        int32_t value = 0;
        if (!ToValue(rawValue, value)) {
            return false;
        }
        if (!mUnit.empty()) {
            std::string_view unit = mUnit;
            if (rawValue == 1.0 && !mUnitSingular.empty()) {
                unit = mUnitSingular;
            }

            snprintf(outTextBuf.data(), outTextBuf.size(), "%d %s", value, unit.data());
            return true;
        } else {
            snprintf(outTextBuf.data(), outTextBuf.size(), "%d", value);
            return true;
        }
        return false;
    }
    bool FromText(std::string_view text, double& outRawValue) const override {
        int32_t parsed = static_cast<int32_t>(std::strtod(text.data(), nullptr));
        return FromValue(parsed, outRawValue);
    }
#ifdef KITSBLIPS_ENABLE_GUI
    bool OnImgui(double& inOutRawValue) const override {
        int32_t value = inOutRawValue;
        bool changed = ImGui::SliderInt(mName.data(), &value, mMin, mMax);
        inOutRawValue = value;
        return changed;
    }
#endif
    bool ToValue(double rawValue, int32_t& out) const {
        out = static_cast<int32_t>(rawValue);
        return true;
    }
    bool FromValue(int32_t in, double& outRaw) const {
        outRaw = static_cast<double>(in);
        return true;
    }

   private:
    const std::string_view mModule;
    const std::string_view mName;
    const int32_t mMin;
    const int32_t mMax;
    const int32_t mDefaultValue;
    const std::string_view mUnit;
    const std::string_view mUnitSingular;
};

/**
 * Represents a selection from a fixed set of options. A handy way to represent these is with dropdown values.
 */
template <typename EnumType>
class EnumParam : public BaseParam {
   public:
    using _valuetype = EnumType;
    EnumParam(std::string_view module,
              std::string_view mName,
              std::vector<std::string_view> mLabels,
              EnumType mDefaultValue)
        : mModule(module), mName(mName), mLabels(std::move(mLabels)), mDefaultValue(mDefaultValue) {}
    bool FillInformation(clap_id id, clap_param_info_t* information) const override {
        memset(information, 0, sizeof(clap_param_info_t));
        information->id = id;
        information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_ENUM | CLAP_PARAM_IS_STEPPED;
        information->min_value = 0;
        information->max_value = static_cast<double>(mLabels.size() - 1);
        information->default_value = GetRawDefault();
        _clapeze_impl::stringCopy(information->name, mName);
        _clapeze_impl::stringCopy(information->module, mModule);

        return true;
    }
    double GetRawDefault() const override {
        double rawDefault;
        assert(FromValue(mDefaultValue, rawDefault));
        return rawDefault;
    }
    bool ToText(double rawValue, etl::span<char>& outTextBuf) const override {
        size_t index = static_cast<size_t>(rawValue);
        if (index < mLabels.size()) {
            snprintf(outTextBuf.data(), outTextBuf.size(), "%s", mLabels[index].data());
            return true;
        }
        return false;
    }
    bool FromText(std::string_view text, double& outRawValue) const override {
        for (size_t index = 0; index < mLabels.size(); ++index) {
            // TODO: trim whitespace, do case-insensitive compare, etc
            if (mLabels[index] == text) {
                outRawValue = static_cast<double>(index);
                return true;
            }
        }
        return false;
    }
#ifdef KITSBLIPS_ENABLE_GUI
    bool OnImgui(double& inOutRawValue) const override {
        EnumType value{};
        ToValue(inOutRawValue, value);
        size_t selectedIndex = static_cast<size_t>(value);
        bool changed = false;
        if (ImGui::BeginCombo(mName.data(), mLabels[selectedIndex].data())) {
            for (size_t idx = 0; idx < mLabels.size(); ++idx) {
                if (ImGui::Selectable(mLabels[idx].data(), idx == selectedIndex)) {
                    FromValue(static_cast<EnumType>(idx), inOutRawValue);
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }
#endif
    bool ToValue(double rawValue, EnumType& out) const {
        size_t index = std::clamp<size_t>(static_cast<size_t>(rawValue), 0, mLabels.size());
        out = static_cast<EnumType>(index);
        return true;
    }
    bool FromValue(EnumType in, double& outRaw) const {
        outRaw = static_cast<double>(in);
        return true;
    }

   private:
    const std::string_view mModule;
    const std::string_view mName;
    const std::vector<std::string_view> mLabels;
    const EnumType mDefaultValue;
};

enum class OnOff { Off, On };
/**
 * Represents a boolean on/off selection
 */
class OnOffParam : public EnumParam<OnOff> {
   public:
    OnOffParam(std::string_view module, std::string_view name, OnOff defaultValue)
        : EnumParam(module, name, {"Off", "On"}, defaultValue) {}
};