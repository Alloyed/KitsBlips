#include "gfx/lights.h"

// IWYU pragma: begin_keep
#include <Corrade/Containers/OptionalStl.h>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/StringStl.h>
// IWYU pragma: end_keep

#include <Magnum/Math/Color.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/SceneData.h>

#include <fmt/format.h>
#include <imgui.h>
#include <optional>
#include <string>

#include "gfx/drawables.h"
#include "gfx/sceneGraph.h"
#include "log.h"

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace Magnum::Math::Literals::ColorLiterals;

namespace kitgui {
void LightInfo::ImGui() const {
    if (ImGui::TreeNode(this, "Light: %s", debugName.c_str())) {
        ImGui::Text("brightness: %f", brightness);
        ImGui::TreePop();
    }
}

Containers::Array<Color3> LightCache::CalculateLightColors() {
    // https://doc.magnum.graphics/magnum/classMagnum_1_1Shaders_1_1PhongGL.html#Shaders-PhongGL-lights
    size_t numLights = mLights.size();
    Containers::Array<Color3> finalLightColors{NoInit, numLights};
    for (size_t i = 0; i < numLights; ++i) {
        const auto& info = mLights[i];
        const auto& light = info.light;
        finalLightColors[i] = light->color() * light->intensity() * info.brightness * mBrightness;
    }
    return finalLightColors;
}

void LightCache::CreateSceneLights(const Trade::SceneData& scene,
                                   std::vector<ObjectInfo>& mSceneObjects,
                                   SceneGraph::DrawableGroup3D& mLightDrawables) {
    /* Import all lights so we know which shaders to instantiate */
    if (scene.hasField(Trade::SceneField::Light)) {
        for (const auto& lightReference : scene.lightsAsArray()) {
            const uint32_t objectId = lightReference.first();
            auto& objectInfo = mSceneObjects[objectId];
            const uint32_t lightId = lightReference.second();
            Object3D* object = objectInfo.object;
            const auto& light = mLights[lightId].light;
            if (!object || !light)
                continue;

            objectInfo.lightIds.push_back(lightId);
            mLightRanges.push_back(light->range());

            /* Add a light drawable, which puts correct camera-relative
               position to lightPositions. Light colors don't change so
               add that directly. */
            new LightDrawable{*object, light->type() == Trade::LightType::Directional ? true : false, mLightPositions,
                              mLightDrawables};
        }
    }
}

void LightCache::LoadLights(Trade::AbstractImporter& importer) {
    /* Load all lights. Lights that fail to load will be NullOpt, saving the
       whole imported data so we can populate the selection info later. */
    //Debug{} << "Loading" << importer.lightCount() << "lights";
    mLights.clear();
    mLights.reserve(importer.lightCount());
    for (uint32_t i = 0; i != importer.lightCount(); ++i) {
        LightInfo lightInfo;
        lightInfo.debugName = importer.lightName(i);
        if (lightInfo.debugName.empty()) {
            lightInfo.debugName = fmt::format("{}", i);
        }

        lightInfo.light = std::optional<Trade::LightData>{importer.light(i)};
        if (!lightInfo.light) {
            //kitgui::log::error(fmt::format("Cannot load light {} {}", i, importer.lightName(i)));
            continue;
        }
        mLights.push_back(std::move(lightInfo));
    }
}
}  // namespace kitgui
