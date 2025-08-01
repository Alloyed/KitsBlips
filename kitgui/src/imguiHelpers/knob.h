#pragma once

namespace kitgui {
namespace ImGuiHelpers {
struct KnobConfig {
    // -1.0f is a sentinel for "pick for me"
    float diameter = -1.0f;
    float minAngleRadians = -1.0f;
    float maxAngleRadians = -1.0f;
    // TODO: fun visuals :)
};
bool knob(const char* id, const KnobConfig& knobConfig, double& rawValueInOut);
}  // namespace ImGuiHelpers
}  // namespace kitgui