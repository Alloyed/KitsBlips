#include "clapeze/ext/parameterConfigs.h"
#include <charconv>
#include <cstring>
#include <string_view>
#include "clap/ext/params.h"
#include "fmt/base.h"

using namespace clapeze;
using namespace clapeze_impl;

bool parseNumberFromText(std::string_view input, double out) {
#ifdef __APPLE__
    // Apple doesn't support charconv properly yet (2026/1/7, apple clang 14)
    std::string tmp;
    out = std::strtod(tmp.c_str(), nullptr);
    return true;
#else
    const char* first = input.data();
    const char* last = input.data() + input.size();
    const auto [ptr, ec] = std::from_chars(first, last, out);
    return ec == std::errc{} && ptr == last;
#endif
}

template<typename... Args>
void formatToSpan(etl::span<char>& buf, fmt::format_string<Args...> fmt, Args&&... args)
{
    // -1 to save space for null terminator
    auto result = fmt::format_to_n(buf.data(), buf.size() - 1, fmt, std::forward<Args>(args)...);
    // now, actually null terminate
    *result.out = '\0';
}

bool NumericParam::FillInformation(clap_id id, clap_param_info_t* information) const {
    memset(information, 0, sizeof(clap_param_info_t));
    information->id = id;
    information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
    information->min_value = 0.0;
    information->max_value = 1.0;
    information->default_value = GetRawDefault();
    stringCopy(information->name, mName);
    stringCopy(information->module, GetModule());
    return true;
}

double NumericParam::GetRawDefault() const {
    double rawDefault{};
    FromValue(mDefaultValue, rawDefault);
    return rawDefault;
}

bool NumericParam::ToText(double rawValue, etl::span<char>& outTextBuf) const {
    float displayValue = 0.0f;
    if (!ToValue(rawValue, displayValue)) {
        return false;
    }
    if (mUnit.empty()) {
        formatToSpan(outTextBuf, "{:.3f}", displayValue);
    } else {
        formatToSpan(outTextBuf, "{:.3f} {}", displayValue, mUnit);
    }
    return true;
}

bool NumericParam::FromText(std::string_view text, double& outRawValue) const {
    double in{};
    if (!parseNumberFromText(text, in)) {
        return false;
    }
    return FromValue(static_cast<float>(in), outRawValue);
}

bool NumericParam::ToValue(double rawValue, float& out) const {
    float curvedValue = mCurve.toCurved(static_cast<float>(rawValue));
    out = std::lerp(mMin, mMax, curvedValue);
    return true;
}

bool NumericParam::FromValue(float in, double& outRaw) const {
    float range = mMax - mMin;
    in = mCurve.fromCurved(in);
    outRaw = range != 0.0f ? (in - mMin) / range : mMin;
    return true;
}

bool PercentParam::ToText(double rawValue, etl::span<char>& outTextBuf) const {
    float displayValue = 0.0f;
    if (!ToValue(rawValue, displayValue)) {
        return false;
    }
    displayValue *= 100.0f;

    formatToSpan(outTextBuf, "{:.2f}%", displayValue);
    return true;
}

bool PercentParam::FromText(std::string_view text, double& outRawValue) const {
    double in{};
    if (!parseNumberFromText(text, in)) {
        return false;
    }
    return FromValue(static_cast<float>(in) / 100.0f, outRawValue);
}

bool IntegerParam::FillInformation(clap_id id, clap_param_info_t* information) const {
    memset(information, 0, sizeof(clap_param_info_t));
    information->id = id;
    information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE | CLAP_PARAM_IS_STEPPED;
    information->min_value = static_cast<double>(mMin);
    information->max_value = static_cast<double>(mMax);
    information->default_value = GetRawDefault();
    stringCopy(information->name, mName);
    return true;
}

double IntegerParam::GetRawDefault() const {
    double rawDefault{};
    FromValue(mDefaultValue, rawDefault);
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
        formatToSpan(outTextBuf,  "{} {}", value, unit);
        return true;
    }

    formatToSpan(outTextBuf, "{}", value);
    return true;
}

bool IntegerParam::FromText(std::string_view text, double& outRawValue) const {
    double in{};
    if (!parseNumberFromText(text, in)) {
        return false;
    }
    return FromValue(static_cast<int32_t>(in), outRawValue);
}

bool IntegerParam::ToValue(double rawValue, int32_t& out) {
    out = static_cast<int32_t>(rawValue);
    return true;
}

bool IntegerParam::FromValue(int32_t in, double& outRaw) {
    outRaw = static_cast<double>(in);
    return true;
}