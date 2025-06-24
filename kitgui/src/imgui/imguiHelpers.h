#pragma once
#ifdef KITSBLIPS_ENABLE_GUI

#include <imgui.h>

namespace kitgui {

namespace helpers {
template <typename FN>
inline void beginFullscreen(FN fn) {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    static bool open = true;
    ImGui::Begin("main", &open,
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);
    {
        fn();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
}  // namespace helpers
}  // namespace kitgui
#endif