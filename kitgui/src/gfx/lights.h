#pragma once

#include <Magnum/GL/Texture.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/MaterialData.h>

#include <optional>
#include <string>
#include <vector>

#include "gfx/sceneGraph.h"

namespace kitgui {

struct LightInfo {
    std::optional<Magnum::Trade::LightData> light;
    std::string debugName;
};

struct LightCache {
    std::vector<LightInfo> mLights;
    size_t mLightCount{};
    bool mShadeless{false};
    float mBrightness{0.5f};
    std::vector<Magnum::Vector4> mLightPositions;
    std::vector<Magnum::Color3> mLightColors;

    Magnum::Containers::Array<Magnum::Color3> calculateLightColors();
    void CreateDefaultLightsIfNecessary(Object3D* mCameraObject, Magnum::SceneGraph::DrawableGroup3D& mLightDrawables);
    void CreateSceneLights(const Magnum::Trade::SceneData& scene,
                           std::vector<ObjectInfo>& mSceneObjects,
                           Magnum::SceneGraph::DrawableGroup3D& mLightDrawables);
    void LoadLightInfo(Magnum::Trade::AbstractImporter& importer);
};
}  // namespace kitgui