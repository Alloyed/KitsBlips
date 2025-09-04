#pragma once
#include <imgui.h>

namespace kitgui {

// So named to look like vanilla imgui
namespace ImGuiHelpers {
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
}  // namespace ImGuiHelpers
}  // namespace kitgui