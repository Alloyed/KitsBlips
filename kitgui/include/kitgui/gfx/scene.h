#pragma once

#include <memory>
#include <string_view>
#include "kitgui/types.h"

namespace kitgui {
class Context;
/**
 * A scene represents a drawable instance of a loaded gltf/glb scene.
 * gltf scenes include drawable geometry, like 3d models, as well as camera and lighting information.
 * you can even use gltf to represent a complex 2d scene: just set it in orthographic mode!
 */
class Scene {
   public:
    explicit Scene(kitgui::Context& mContext);
    ~Scene();
    /** Synchronously loads the scene, including all transitive resources. */
    void Load(std::string_view path);
    /** Updates any time-variant parts of the scene, such as animations. */
    void Update();
    /** Draws the scene. */
    void Draw();

    void PlayAnimationByName(std::string_view name);
    void SetObjectRotationByName(std::string_view name, const kitgui::Quaternion& rot);

   private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
}  // namespace kitgui
