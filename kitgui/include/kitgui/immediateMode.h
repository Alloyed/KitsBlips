#pragma once

namespace kitgui {
class DomScene;
bool ImGuiKnob(const char* label, float* value, float min = 0.0f, float max = 1.0f, float step = 0.01f);
bool ImGuiScene(const char* label, DomScene& scene);

}  // namespace kitgui