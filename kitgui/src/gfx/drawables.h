#pragma once

#include <Corrade/Containers/ArrayView.h>

#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/TranslationRotationScalingTransformation3D.h>
#include <Magnum/Shaders/FlatGL.h>
#include <Magnum/Shaders/MeshVisualizerGL.h>
#include <Magnum/Shaders/PhongGL.h>

#include <span>
#include <vector>

#include "Magnum/Magnum.h"
#include "gfx/meshes.h"
#include "gfx/sceneGraph.h"

namespace kitgui {
struct MaterialCache;
}

namespace kitgui {

class BaseDrawable : public Magnum::SceneGraph::Drawable3D {
   public:
    explicit BaseDrawable(Object3D& object, Magnum::SceneGraph::DrawableGroup3D* group)
        : Magnum::SceneGraph::Drawable3D(object, group) {}
    virtual void ImGui() = 0;
};

class FlatDrawable : public BaseDrawable {
   public:
    explicit FlatDrawable(Object3D& object,
                          Magnum::Shaders::FlatGL3D& shader,
                          Magnum::GL::Mesh& mesh,
                          uint32_t objectId,
                          const Magnum::Color4& color,
                          const Magnum::Vector3& scale,
                          Magnum::SceneGraph::DrawableGroup3D& group);

   private:
    void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& camera) override;
    void ImGui() override;

    Magnum::Shaders::FlatGL3D& mShader;
    Magnum::GL::Mesh& mMesh;
    uint32_t mObjectId;
    Magnum::Color4 mColor;
    Magnum::Vector3 mScale;
};

class PhongDrawable : public BaseDrawable {
   public:
    explicit PhongDrawable(Object3D& object,
                           Magnum::Shaders::PhongGL& shader,
                           Magnum::GL::Mesh& mesh,
                           uint32_t objectId,
                           const Magnum::Color4& color,
                           Magnum::GL::Texture2D* diffuseTexture,
                           Magnum::GL::Texture2D* normalTexture,
                           float normalTextureScale,
                           float alphaMask,
                           const Magnum::Matrix3& textureMatrix,
                           const bool& shadeless,
                           Magnum::SceneGraph::DrawableGroup3D& group);

    explicit PhongDrawable(Object3D& object,
                           Magnum::Shaders::PhongGL& shader,
                           Magnum::GL::Mesh& mesh,
                           uint32_t objectId,
                           const Magnum::Color4& color,
                           const bool& shadeless,
                           Magnum::SceneGraph::DrawableGroup3D& group);

   private:
    void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& camera) override;
    void ImGui() override;

    Magnum::Shaders::PhongGL& mShader;
    Magnum::GL::Mesh& mMesh;
    uint32_t mObjectId;
    Magnum::Color4 mColor;
    Magnum::GL::Texture2D* mDiffuseTexture;
    Magnum::GL::Texture2D* mNormalTexture;
    float mNormalTextureScale;
    float mAlphaMask;
    Magnum::Matrix3 mTextureMatrix;
    bool mShadeless;
};

class LightDrawable : public BaseDrawable {
   public:
    explicit LightDrawable(Object3D& object,
                           bool directional,
                           std::vector<Magnum::Vector4>& positions,
                           Magnum::SceneGraph::DrawableGroup3D& group);

   private:
    void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D&) override;
    void ImGui() override;

    bool mDirectional;
    std::vector<Magnum::Vector4>& mPositions;
};

struct DrawableCache {
    Magnum::Shaders::FlatGL3D& FlatShader(Magnum::Shaders::FlatGL3D::Flags flags);
    Magnum::Shaders::PhongGL& PhongShader(Magnum::Shaders::PhongGL::Flags flags, uint32_t lightCount);
    Magnum::SceneGraph::Drawable3D* CreateDrawableFromMesh(MaterialCache& mMaterialCache,
                                                           MeshInfo& meshInfo,
                                                           ObjectInfo& objectInfo,
                                                           int32_t materialId,
                                                           uint32_t lightCount,
                                                           bool shadeless);

    void SetLightColors(const std::span<const Magnum::Color3>& colors) {
        const Corrade::Containers::ArrayView<const Magnum::Color3> colorsView(colors.data(), colors.size());
        for (auto& pair : mPhongShaders) {
            pair.second.setLightColors(colorsView);
            pair.second.setLightSpecularColors(colorsView);
        }
    }

    void SetLightPositions(const std::span<const Magnum::Vector4>& positions) {
        const Corrade::Containers::ArrayView<const Magnum::Vector4> positionsView(positions.data(), positions.size());
        for (auto& pair : mPhongShaders) {
            pair.second.setLightPositions(positionsView);
        }
    }
    void SetLightRanges(const std::span<const float>& ranges) {
        const Corrade::Containers::ArrayView<const float> rangesView(ranges.data(), ranges.size());
        for (auto& pair : mPhongShaders) {
            pair.second.setLightRanges(rangesView);
        }
    }

    // uint32_t = Shaders::FlatGL3D::Flags
    std::unordered_map<uint32_t, Magnum::Shaders::FlatGL3D> mFlatShaders;
    // uint32_t = Shaders::PhongGL::Flags
    std::unordered_map<uint32_t, Magnum::Shaders::PhongGL> mPhongShaders;
    // uint32_t = Shaders::MeshVisualizer3D::Flags
    std::unordered_map<uint32_t, Magnum::Shaders::MeshVisualizerGL3D> mMeshVisualizerShaders;

    Magnum::SceneGraph::DrawableGroup3D mOpaqueDrawables;
    Magnum::SceneGraph::DrawableGroup3D mTransparentDrawables;
    Magnum::SceneGraph::DrawableGroup3D mLightDrawables;
};

}  // namespace kitgui
