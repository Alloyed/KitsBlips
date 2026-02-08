#include <imgui.h>
#include "gfx/sceneGraph.h"
#include "gfx/lights.h"
#include "gfx/meshes.h"

namespace kitgui {
    ObjectInfo::ObjectInfo() = default;
    void ObjectInfo::ImGui(const std::vector<ObjectInfo>& objects, const std::vector<MeshInfo>& meshes, const std::vector<LightInfo>& lights) const {
        if(ImGui::TreeNode(this, "Object: %s", name.c_str())) {
            ImGui::LabelText("id", "%u", id);
            for(const uint32_t id : childIds) {
                objects[id].ImGui(objects, meshes, lights);
            }
            for(const uint32_t id : meshIds) {
                meshes[id].ImGui();
            }
            for(const uint32_t id : lightIds) {
                lights[id].ImGui();
            }
            for(const uint32_t id : cameraIds) {
                ImGui::PushID(id);
                ImGui::LabelText("camera", "%u", id);
                ImGui::PopID();
            }

            ImGui::TreePop();
        }
    }
}
