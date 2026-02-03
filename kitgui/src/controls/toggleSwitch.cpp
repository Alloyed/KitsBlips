#include "kitgui/controls/toggleSwitch.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <string>
#include <algorithm>

namespace kitgui {
bool ToggleSwitch::Update(bool& rawValueInOut) {
    // roughly adapted from https://github.com/ocornut/imgui/issues/1537

    auto itemWidth = mWidth ? (*mWidth) * ImGui::GetIO().FontGlobalScale : ImGui::GetTextLineHeight() * 2.0f;
    ImVec2 screen_pos = mPos ? ImVec2{mPos->x(), mPos->y()} : ImGui::GetCursorScreenPos();

    ImGui::PushID(static_cast<void*>(this));
    ImGui::PushItemWidth(itemWidth);
    ImGui::BeginGroup();
    ImGui::SetCursorScreenPos(screen_pos);

    bool value_changed = false;
    if(ImGui::InvisibleButton("button", {itemWidth, itemWidth})) {
        value_changed = true;
        rawValueInOut = !rawValueInOut;
    }

    // responses
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
        // descriptive tooltip
        ImGui::SetTooltip("%s", GetName().c_str());
    }

    if (mShowDebug) {
        // Drawing using imgui primitives

        /*
        auto* colors = ImGui::GetStyle().Colors;
        ImColor base = colors[ImGuiCol_ButtonActive];
        ImColor hovered = colors[ImGuiCol_ButtonHovered];
        ImColor active = colors[ImGuiCol_ButtonHovered];
        ImColor base2 = ImVec4(colors[ImGuiCol_ButtonActive].x * 0.5f, colors[ImGuiCol_ButtonActive].y * 0.5f,
                colors[ImGuiCol_ButtonActive].z * 0.5f, colors[ImGuiCol_ButtonActive].w);
        ImColor hovered2 = ImVec4(colors[ImGuiCol_ButtonHovered].x * 0.5f, colors[ImGuiCol_ButtonHovered].y * 0.5f,
                colors[ImGuiCol_ButtonHovered].z * 0.5f, colors[ImGuiCol_ButtonHovered].w);
        ImColor active2 = ImVec4(colors[ImGuiCol_ButtonHovered].x * 0.5f, colors[ImGuiCol_ButtonHovered].y * 0.5f,
                colors[ImGuiCol_ButtonHovered].z * 0.5f, colors[ImGuiCol_ButtonHovered].w);
        */

        ImU32 col_bg;
        if (ImGui::IsItemHovered())
            col_bg = rawValueInOut ? IM_COL32(145+20, 211, 68+20, 255) : IM_COL32(218-20, 218-20, 218-20, 255);
        else
            col_bg = rawValueInOut ? IM_COL32(145, 211, 68, 255) : IM_COL32(218, 218, 218, 255);

        ImGui::GetWindowDrawList()->AddRectFilled(screen_pos, ImVec2(screen_pos.x + itemWidth, screen_pos.y + itemWidth), col_bg, 0.0f);
        //draw_list->AddCircleFilled(ImVec2(rawValueInOut ? (p.x + width - radius) : (p.x + radius), p.y + radius), radius - 1.5f, IM_COL32(255, 255, 255, 255));
    }

    ImGui::EndGroup();
    ImGui::PopItemWidth();
    ImGui::PopID();

    return value_changed;
}
}  // namespace kitgui
