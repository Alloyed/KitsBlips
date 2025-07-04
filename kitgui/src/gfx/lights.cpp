#include "gfx/lights.h"

// enable conversion to stl
#include <Corrade/Containers/OptionalStl.h>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/StringStl.h>
// done

#include <Magnum/Math/Color.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/SceneData.h>

#include <format>
#include <optional>
#include <string>

#include "gfx/drawables.h"
#include "gfx/sceneGraph.h"

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace Magnum::Math::Literals::ColorLiterals;

namespace kitgui {

Containers::Array<Color3> LightCache::calculateLightColors() {
    Containers::Array<Color3> lightColorsBrightness{NoInit, mLightColors.size()};
    for (size_t i = 0; i != lightColorsBrightness.size(); ++i) {
        lightColorsBrightness[i] = mLightColors[i] * mBrightness;
    }
    return lightColorsBrightness;
}

void LightCache::CreateDefaultLightsIfNecessary(Object3D* mCameraObject, SceneGraph::DrawableGroup3D& mLightDrawables) {
    if (mLightCount == 0) {
        mLightCount = 3;

        Object3D* first = new Object3D{mCameraObject};
        first->translate({10.0f, 10.0f, 10.0f});
        new LightDrawable{*first, true, mLightPositions, mLightDrawables};

        Object3D* second = new Object3D{mCameraObject};
        first->translate(Vector3{-5.0f, -5.0f, 10.0f} * 100.0f);
        new LightDrawable{*second, true, mLightPositions, mLightDrawables};

        Object3D* third = new Object3D{mCameraObject};
        third->translate(Vector3{0.0f, 10.0f, -10.0f} * 100.0f);
        new LightDrawable{*third, true, mLightPositions, mLightDrawables};

        mLightColors = {0xffffff_rgbf, 0xffcccc_rgbf, 0xccccff_rgbf};
    }
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

            ++mLightCount;

            /* Save the light pointer as well, so we know what to print for
               object selection. Lights have their own info text, so not
               setting the type. */
            /** @todo this doesn't handle multi-light objects */
            objectInfo.lightId = lightId;

            /* Add a light drawable, which puts correct camera-relative
               position to lightPositions. Light colors don't change so
               add that directly. */
            new LightDrawable{*object, light->type() == Trade::LightType::Directional ? true : false, mLightPositions,
                              mLightDrawables};
            mLightColors.push_back(light->color() * light->intensity());
        }
    }
}

void LightCache::LoadLightInfo(Trade::AbstractImporter& importer) {
    /* Load all lights. Lights that fail to load will be NullOpt, saving the
       whole imported data so we can populate the selection info later. */
    Debug{} << "Loading" << importer.lightCount() << "lights";
    mLights.clear();
    mLights.reserve(importer.lightCount());
    for (uint32_t i = 0; i != importer.lightCount(); ++i) {
        LightInfo lightInfo;
        lightInfo.debugName = importer.lightName(i);
        if (lightInfo.debugName.empty()) {
            lightInfo.debugName = std::format("{}", i);
        }

        auto light = std::optional<Trade::LightData>{importer.light(i)};
        if (!light) {
            Warning{} << "Cannot load light" << i << importer.lightName(i);
            continue;
        }
        mLights.push_back(std::move(lightInfo));
    }
}
}  // namespace kitgui