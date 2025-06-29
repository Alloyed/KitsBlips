#pragma once

namespace clapeze {
class NumericParam;
}

namespace kitgui {

struct KnobConfig {
    // -1.0f is a sentinel for "pick for me"
    float diameter = -1.0f;
    float minAngleRadians = -1.0f;
    float maxAngleRadians = -1.0f;
    // TODO: fun visuals :)
};
// imgui
bool knob(const char* id, const clapeze::NumericParam& param, const KnobConfig& knobConfig, double& rawValueInOut);

}  // namespace kitgui