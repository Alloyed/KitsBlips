#include "clapeze/ext/parameterConfigs.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string_view>

using namespace clapeze;
using namespace clapeze_impl;

bool NumericParam::FillInformation(clap_id id, clap_param_info_t* information) const {
    memset(information, 0, sizeof(clap_param_info_t));
    information->id = id;
    information->flags = CLAP_PARAM_IS_AUTOMATABLE;
    information->min_value = 0.0;
    information->max_value = 1.0;
    information->default_value = GetRawDefault();
    stringCopy(information->name, mName);
    stringCopy(information->module, GetModule());
    return true;
}

double NumericParam::GetRawDefault() const {
    double rawDefault{};
    assert(FromValue(mDefaultValue, rawDefault));
    return rawDefault;
}

bool NumericParam::ToText(double rawValue, etl::span<char>& outTextBuf) const {
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

bool NumericParam::FromText(std::string_view text, double& outRawValue) const {
    double in = std::strtod(text.data(), nullptr);
    return FromValue(static_cast<float>(in), outRawValue);
}

bool NumericParam::ToValue(double rawValue, float& out) const {
    out = std::lerp(mMin, mMax, static_cast<float>(rawValue));
    return true;
}

bool NumericParam::FromValue(float in, double& outRaw) const {
    float range = mMax - mMin;
    outRaw = range != 0.0f ? (in - mMin) / range : mMin;
    return true;
}

bool PercentParam::ToText(double rawValue, etl::span<char>& outTextBuf) const {
    float displayValue = static_cast<float>(rawValue * 100.0f);
    snprintf(outTextBuf.data(), outTextBuf.size(), "%.2f%%", displayValue);
    return true;
}

bool PercentParam::FromText(std::string_view text, double& outRawValue) const {
    double in = std::strtod(text.data(), nullptr);
    return FromValue(static_cast<float>(in) / 100.0f, outRawValue);
}

bool IntegerParam::FillInformation(clap_id id, clap_param_info_t* information) const {
    memset(information, 0, sizeof(clap_param_info_t));
    information->id = id;
    information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
    information->min_value = static_cast<double>(mMin);
    information->max_value = static_cast<double>(mMax);
    information->default_value = GetRawDefault();
    stringCopy(information->name, mName);
    return true;
}

double IntegerParam::GetRawDefault() const {
    double rawDefault{};
    assert(FromValue(mDefaultValue, rawDefault));
    return rawDefault;
}

bool IntegerParam::ToText(double rawValue, etl::span<char>& outTextBuf) const {
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
    }

    snprintf(outTextBuf.data(), outTextBuf.size(), "%d", value);
    return true;
}

bool IntegerParam::FromText(std::string_view text, double& outRawValue) const {
    int32_t parsed = static_cast<int32_t>(std::strtod(text.data(), nullptr));
    return FromValue(parsed, outRawValue);
}

bool IntegerParam::ToValue(double rawValue, int32_t& out) const {
    out = static_cast<int32_t>(rawValue);
    return true;
}

bool IntegerParam::FromValue(int32_t in, double& outRaw) const {
    outRaw = static_cast<double>(in);
    return true;
}