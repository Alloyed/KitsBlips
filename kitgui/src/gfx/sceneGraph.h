#pragma once
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/TranslationRotationScalingTransformation3D.h>
#include <cstdint>
#include <string>

namespace kitgui {

using Object3D = Magnum::SceneGraph::Object<Magnum::SceneGraph::TranslationRotationScalingTransformation3D>;
using Scene3D = Magnum::SceneGraph::Scene<Magnum::SceneGraph::TranslationRotationScalingTransformation3D>;

struct ObjectInfo {
    uint32_t id;
    Object3D* object;
    std::string debugName;
    uint32_t lightId{0xffffffffu};
};

}  // namespace kitgui
