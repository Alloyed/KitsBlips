#include "clapeze/features/params/parameterTypes.h"

#include <string_view>
#include "clapeze/impl/stringUtils.h"

using namespace clapeze;

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
        impl::formatToSpan(outTextBuf, "{:.3f}", displayValue);
    } else {
        impl::formatToSpan(outTextBuf, "{:.3f} {}", displayValue, mUnit);
    }
    return true;
}

bool NumericParam::FromText(std::string_view text, double& outRawValue) const {
    double in{};
    if (!impl::parseNumberFromText(text, in)) {
        return false;
    }
    return FromValue(static_cast<float>(in), outRawValue);
}

bool NumericParam::ToValue(double rawValue, float& out) const {
    float curvedValue = mCurve.toCurved(static_cast<float>(rawValue));
    out = std::clamp(std::lerp(mMin, mMax, curvedValue), mMin, mMax);
    return true;
}

bool NumericParam::FromValue(float in, double& outRaw) const {
    float range = mMax - mMin;
    in = mCurve.fromCurved(in);
    outRaw = std::clamp(range != 0.0f ? (in - mMin) / range : mMin, 0.0f, 1.0f);
    return true;
}

/* static */ double NumericParam::ConvertToRange(double inRaw,
                                                 float fromMin,
                                                 float fromMax,
                                                 const ParamCurve& fromCurve,
                                                 float toMin,
                                                 float toMax,
                                                 const ParamCurve& toCurve) {
    NumericParam tmpFrom("", "", fromMin, fromMax, 0.0f);
    tmpFrom.mCurve = fromCurve;
    NumericParam tmpTo("", "", toMin, toMax, 0.0f);
    tmpFrom.mCurve = toCurve;

    float realValue{};
    double outRaw{};
    tmpFrom.ToValue(inRaw, realValue);
    tmpTo.FromValue(realValue, outRaw);
    return outRaw;
}

bool PercentParam::ToText(double rawValue, etl::span<char>& outTextBuf) const {
    float displayValue = 0.0f;
    if (!ToValue(rawValue, displayValue)) {
        return false;
    }
    displayValue *= 100.0f;

    impl::formatToSpan(outTextBuf, "{:.2f}%", displayValue);
    return true;
}

bool PercentParam::FromText(std::string_view text, double& outRawValue) const {
    double in{};
    if (!impl::parseNumberFromText(text, in)) {
        return false;
    }
    return FromValue(static_cast<float>(in) / 100.0f, outRawValue);
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
        impl::formatToSpan(outTextBuf, "{} {}", value, unit);
        return true;
    }

    impl::formatToSpan(outTextBuf, "{}", value);
    return true;
}

bool IntegerParam::FromText(std::string_view text, double& outRawValue) const {
    double in{};
    if (!impl::parseNumberFromText(text, in)) {
        return false;
    }
    return FromValue(static_cast<int32_t>(in), outRawValue);
}

bool IntegerParam::ToValue(double rawValue, int32_t& out) const {
    out = std::clamp(static_cast<int32_t>(rawValue), mMin, mMax);
    return true;
}

bool IntegerParam::FromValue(int32_t in, double& outRaw) const {
    outRaw = static_cast<double>(std::clamp(in, mMin, mMax));
    return true;
}
