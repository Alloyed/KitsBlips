#pragma once

#include <clap/clap.h>
#include <clap/ext/params.h>
#include <etl/span.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include <clapeze/ext/parameters.h>

namespace clapeze_impl {
/* safe alternative to strcpy when the buffer size is known at compile time. assumes the buffer is zero'd out in
 * advance*/
template <size_t BUFFER_SIZE>
void stringCopy(char (&buffer)[BUFFER_SIZE], std::string_view src) {
    static_assert(BUFFER_SIZE > 0);
    // copy at most BUFFER_SIZE-1 bytes. the last byte is reserved for the null terminator
    // NOLINTNEXTLINE
    std::memcpy(buffer, src.data(), std::min(src.length(), BUFFER_SIZE - 1));
}
}  // namespace clapeze_impl

namespace clapeze {
/**
 * Represents a numeric value. these are always mapped 0-1 on the DAW side so we can adjust the response curve to the
 * user's taste.
 */
struct NumericParam : public BaseParam {
   public:
    using _valuetype = float;
    NumericParam(std::string_view mName, float mMin, float mMax, float mDefaultValue, std::string_view mUnit = "")
        : mName(mName), mMin(mMin), mMax(mMax), mDefaultValue(mDefaultValue), mUnit(mUnit) {}
    bool FillInformation(clap_id id, clap_param_info_t* information) const override;
    double GetRawDefault() const override;
    bool ToText(double rawValue, etl::span<char>& outTextBuf) const override;
    bool FromText(std::string_view text, double& outRawValue) const override;
    bool ToValue(double rawValue, float& out) const;
    bool FromValue(float in, double& outRaw) const;

    const std::string mName;
    const float mMin;
    const float mMax;
    const float mDefaultValue;
    const std::string mUnit;
};

struct PercentParam : public NumericParam {
   public:
    using _valuetype = float;
    PercentParam(std::string_view mName, float mDefaultValue) : NumericParam(mName, 0.0f, 1.0f, mDefaultValue) {}
    PercentParam(std::string_view mName, float minValue, float maxValue, float mDefaultValue)
        : NumericParam(mName, minValue, maxValue, mDefaultValue) {}

    bool ToText(double rawValue, etl::span<char>& outTextBuf) const override;
    bool FromText(std::string_view text, double& outRawValue) const override;
};

struct DbParam : public NumericParam {
   public:
    DbParam(std::string_view name, float minValue, float maxValue, float mDefaultValue)
        : NumericParam(name, minValue, maxValue, mDefaultValue, "db") {}
};

/**
 * Represents an integer value. handy for managing the counts of things (eg. number of voices)
 */
struct IntegerParam : public BaseParam {
   public:
    using _valuetype = int32_t;
    IntegerParam(std::string_view mName,
                 int32_t mMin,
                 int32_t mMax,
                 int32_t mDefaultValue,
                 std::string_view mUnit = "",
                 std::string_view mUnitSingular = "")
        : mName(mName),
          mMin(mMin),
          mMax(mMax),
          mDefaultValue(mDefaultValue),
          mUnit(mUnit),
          mUnitSingular(mUnitSingular) {}

    bool FillInformation(clap_id id, clap_param_info_t* information) const override;
    double GetRawDefault() const override;
    bool ToText(double rawValue, etl::span<char>& outTextBuf) const override;
    bool FromText(std::string_view text, double& outRawValue) const override;
    bool ToValue(double rawValue, int32_t& out) const;
    bool FromValue(int32_t in, double& outRaw) const;

    const std::string mName;
    const int32_t mMin;
    const int32_t mMax;
    const int32_t mDefaultValue;
    const std::string mUnit;
    const std::string mUnitSingular;
};

/**
 * Represents a selection from a fixed set of options. A handy way to represent these is with dropdown values.
 */
template <typename TEnum>
struct EnumParam : public BaseParam {
   public:
    using _valuetype = TEnum;
    EnumParam(std::string_view mName, const std::vector<std::string>& mLabels, TEnum mDefaultValue)
        : mName(mName), mLabels(mLabels), mDefaultValue(mDefaultValue) {}
    bool FillInformation(clap_id id, clap_param_info_t* information) const override {
        memset(information, 0, sizeof(clap_param_info_t));
        information->id = id;
        information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_ENUM | CLAP_PARAM_IS_STEPPED;
        information->min_value = 0;
        information->max_value = static_cast<double>(mLabels.size() - 1);
        information->default_value = GetRawDefault();
        clapeze_impl::stringCopy(information->name, mName);
        clapeze_impl::stringCopy(information->module, GetModule());

        return true;
    }
    double GetRawDefault() const override {
        double rawDefault{};
        FromValue(mDefaultValue, rawDefault);
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
    bool ToValue(double rawValue, TEnum& out) const {
        size_t index = std::clamp<size_t>(static_cast<size_t>(rawValue), 0, mLabels.size());
        out = static_cast<TEnum>(index);
        return true;
    }
    bool FromValue(TEnum in, double& outRaw) const {
        outRaw = static_cast<double>(in);
        return true;
    }

    const std::string mName;
    const std::vector<std::string> mLabels;
    const TEnum mDefaultValue;
};

enum class OnOff : uint8_t { Off, On };
/**
 * Represents a boolean on/off selection
 */
struct OnOffParam : public EnumParam<OnOff> {
   public:
    OnOffParam(std::string_view name, OnOff defaultValue) : EnumParam(name, {"Off", "On"}, defaultValue) {}
};
}  // namespace clapeze
