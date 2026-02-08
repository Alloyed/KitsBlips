#pragma once

#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector.h>
#include <Magnum/Trade/LightData.h>

#include <optional>
#include <string>
#include <vector>

#include "gfx/sceneGraph.h"

// Forward declarations
namespace Magnum {
namespace Trade {
class SceneData;
class AbstractImporter;
}  // namespace Trade
}  // namespace Magnum

namespace kitgui {

struct LightInfo {
    std::optional<Magnum::Trade::LightData> light;
    float brightness = 1.0f;
    std::string debugName{};
    void ImGui() const;
};

struct LightCache {
    std::vector<LightInfo> mLights;
    bool mShadeless{false};
    float mBrightness{0.0025f};
    // calculated externally, in LightDrawable
    std::vector<Magnum::Vector4> mLightPositions;
    std::vector<float> mLightRanges;

    Magnum::Containers::Array<Magnum::Color3> CalculateLightColors();
    void CreateSceneLights(const Magnum::Trade::SceneData& scene,
                           std::vector<ObjectInfo>& mSceneObjects,
                           Magnum::SceneGraph::DrawableGroup3D& mLightDrawables);
    void LoadLights(Magnum::Trade::AbstractImporter& importer);
};
}  // namespace kitgui
