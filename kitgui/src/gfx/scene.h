#pragma once

#include <Magnum/Trade/AbstractImporter.h>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/BitArray.h>
#include <Corrade/Containers/OptionalStl.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/Utility/Algorithms.h>

#include <Magnum/Animation/Player.h>
#include <Magnum/Animation/Track.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/CubicHermite.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/AnimationData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>

#include <fmt/format.h>
#include <cassert>
#include <chrono>

#include "fileContext.h"
#include "gfx/drawables.h"
#include "gfx/lights.h"
#include "gfx/materials.h"
#include "gfx/meshes.h"
#include "gfx/sceneGraph.h"
#include "kitgui/context.h"

namespace kitgui {
class Context;
struct DomSceneImpl {
    explicit DomSceneImpl(kitgui::Context& mContext) : mContext(mContext) {}
    void Load(std::string_view path);
    void LoadImpl(Magnum::Trade::AbstractImporter& importer, std::string_view debugName);
    void Update();
    void Draw();

    kitgui::Context& mContext;

    MeshCache mMeshCache;
    Magnum::SceneGraph::DrawableGroup3D mLightDrawables;

    MaterialCache mMaterialCache;
    LightCache mLightCache;
    DrawableCache mDrawableCache;

    Scene3D mScene;
    std::vector<ObjectInfo> mSceneObjects;
    Object3D* mCameraObject{};
    Magnum::SceneGraph::Camera3D* mCamera;

    Magnum::Animation::Player<std::chrono::nanoseconds, float> mPlayer;
};
}  // namespace kitgui
