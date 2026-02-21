#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include "kitgui/types.h"

namespace kitgui {
class Context;

struct ObjectScreenPosition {
    kitgui::Vector2 pos;
    kitgui::Vector2 size;
};

struct SceneTweakables {
    /**if true, ignore all light features and draw everything at max brightness. also known as "fullbright"  */
    bool shadeless = false;
    /** modifies the contribution of dynamic lights to the scene */
    float dynamicLightFactor = 0.0005f;
    /**
     * modifies the contribution of ambient/indirect light to the scene. this is a uniform light applied to everything,
     * and at 1.0 this looks the same as "fullbright"
     */
    float ambientLightFactor = 0.1f;

    /** modifies the contribution of emissive bloom to the scene */
    float bloomIntensity = 1.0f;

    /**
     * modifies the pre-tonemapper light colors.
     * more exposure == more detail in darker areas of the scene (good for night)
     * less exposure == more detail in brighter areas (good for daytime)
     */
    float hdrExposureFactor = 1.0f;
    /**
     * modifies the display-specific gamma mapping. 2.2 is standard sRGB. but monitors aren't perfect so the 2.2 needs a
     * bit of user-controlled wiggle room.
     */
    float gamma = 2.2f;
};

/**
 * A scene represents a drawable instance of a loaded gltf/glb scene.
 * gltf scenes include drawable geometry, like 3d models, as well as camera and lighting information.
 * you can even use gltf to represent a complex 2d scene: just set it in orthographic mode!
 *
 * The built-in shader is Magnum::Shaders::PhongGL, which is notably _not_ PBR. We do our best, anyways.
 */
class Scene {
   public:
    enum class Axis { X, Y, Z };
    explicit Scene(kitgui::Context& mContext);
    ~Scene();
    /** Synchronously loads the scene, including all transitive resources. */
    void Load(std::string_view path);
    /** Updates any time-variant parts of the scene, such as animations. */
    void Update();
    /** Draws the scene. */
    void Draw();
    /** Show debug UI elements for scene */
    void ImGui();
    /** Sets the viewport size, recalculating any cameras as needed. */
    void SetViewport(const kitgui::Vector2& size);

    SceneTweakables& GetSceneTweakables();
    void ApplySceneTweakables();

    // These methods are added as needed, so no rhyme or reason to what's supported really
    void PlayAnimationByName(std::string_view name);
    void SetObjectRotationByName(std::string_view name, float angleRadians, Axis axis);
    void SetLedBrightnessByName(std::string_view name, float emission);
    std::optional<ObjectScreenPosition> GetObjectScreenPositionByName(std::string_view name);

   private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
}  // namespace kitgui
