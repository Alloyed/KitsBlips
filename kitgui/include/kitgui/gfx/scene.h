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
    /** Sets scene overall brightness, from 0-1+. nullopt means fullbright. */
    void SetBrightness(std::optional<float> brightness);
    /** Sets scene ambient brightness, from 0-1+. nullopt means fullbright. */
    void SetAmbientBrightness(float brightness);

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
