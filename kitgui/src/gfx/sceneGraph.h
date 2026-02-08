#pragma once
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/TranslationRotationScalingTransformation3D.h>
#include <cstdint>
#include <string>


namespace kitgui {

using Object3D = Magnum::SceneGraph::Object<Magnum::SceneGraph::TranslationRotationScalingTransformation3D>;
using Scene3D = Magnum::SceneGraph::Scene<Magnum::SceneGraph::TranslationRotationScalingTransformation3D>;

struct MeshInfo;
struct LightInfo;

struct ObjectInfo {
    uint32_t id;
    Object3D* object;
    std::string name;

    // for reverse lookup
    std::vector<uint32_t> parentIds;
    std::vector<uint32_t> childIds;
    std::vector<uint32_t> meshIds;
    std::vector<uint32_t> lightIds;
    std::vector<uint32_t> cameraIds;

    ObjectInfo(); // for forward decl only
    void ImGui(const std::vector<ObjectInfo>& objects, const std::vector<MeshInfo>& meshes, const std::vector<LightInfo>& lights) const;
};

}  // namespace kitgui
