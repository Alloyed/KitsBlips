#include "gfx/drawables.h"

#include <Magnum/GL/Renderer.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/Shaders/FlatGL.h>
#include <Magnum/Shaders/PhongGL.h>
#include <vector>

#include "gfx/sceneGraph.h"

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace Magnum::Math::Literals::ColorLiterals;

namespace kitgui {

FlatDrawable::FlatDrawable(Object3D& object,
                           Shaders::FlatGL3D& shader,
                           GL::Mesh& mesh,
                           uint32_t objectId,
                           const Color4& color,
                           const Vector3& scale,
                           SceneGraph::DrawableGroup3D& group)
    : SceneGraph::Drawable3D{object, &group},
      mShader(shader),
      mMesh(mesh),
      mObjectId{objectId},
      mColor{color},
      mScale{scale} {}

void FlatDrawable::draw(const Matrix4& transformationMatrix, SceneGraph::Camera3D& camera) {
    /* Override the inherited scale, if requested */
    Matrix4 transformation;
    if (mScale == mScale) {
        transformation = Matrix4::from(transformationMatrix.rotationShear(), transformationMatrix.translation()) *
                         Matrix4::scaling(Vector3{mScale});
    } else {
        transformation = transformationMatrix;
    }

    mShader.setColor(mColor)
        .setTransformationProjectionMatrix(camera.projectionMatrix() * transformation)
        .setObjectId(mObjectId);

    mShader.draw(mMesh);
}

PhongDrawable::PhongDrawable(Object3D& object,
                             Shaders::PhongGL& shader,
                             GL::Mesh& mesh,
                             uint32_t objectId,
                             const Color4& color,
                             GL::Texture2D* diffuseTexture,
                             GL::Texture2D* normalTexture,
                             Float normalTextureScale,
                             Float alphaMask,
                             const Matrix3& textureMatrix,
                             const bool& shadeless,
                             SceneGraph::DrawableGroup3D& group)
    : SceneGraph::Drawable3D{object, &group},
      mShader(shader),
      mMesh(mesh),
      mObjectId{objectId},
      mColor{color},
      mDiffuseTexture{diffuseTexture},
      mNormalTexture{normalTexture},
      mNormalTextureScale{normalTextureScale},
      mAlphaMask{alphaMask},
      mTextureMatrix{textureMatrix},
      mShadeless{shadeless} {}

PhongDrawable::PhongDrawable(Object3D& object,
                             Shaders::PhongGL& shader,
                             GL::Mesh& mesh,
                             uint32_t objectId,
                             const Color4& color,
                             const bool& shadeless,
                             SceneGraph::DrawableGroup3D& group)
    : SceneGraph::Drawable3D{object, &group},
      mShader(shader),
      mMesh(mesh),
      mObjectId{objectId},
      mColor{color},
      mDiffuseTexture{nullptr},
      mNormalTexture{nullptr},
      mAlphaMask{0.5f},
      mShadeless{shadeless} {}

void PhongDrawable::draw(const Matrix4& transformationMatrix, SceneGraph::Camera3D& camera) {
    Matrix4 usedTransformationMatrix{NoInit};
    usedTransformationMatrix = transformationMatrix;

    mShader.setTransformationMatrix(usedTransformationMatrix)
        .setNormalMatrix(usedTransformationMatrix.normalMatrix())
        .setProjectionMatrix(camera.projectionMatrix())
        .setObjectId(mObjectId);

    if (mDiffuseTexture) {
        mShader.bindAmbientTexture(*mDiffuseTexture).bindDiffuseTexture(*mDiffuseTexture);
    }
    if (mNormalTexture) {
        mShader.bindNormalTexture(*mNormalTexture).setNormalTextureScale(mNormalTextureScale);
    }

    if (mShadeless) {
        mShader.setAmbientColor(mColor).setDiffuseColor(0x00000000_rgbaf).setSpecularColor(0x00000000_rgbaf);
    } else {
        mShader.setAmbientColor(mColor * 0.06f).setDiffuseColor(mColor).setSpecularColor(0x11111100_rgbaf);
    }

    if (mShader.flags() & Shaders::PhongGL::Flag::TextureTransformation) {
        mShader.setTextureMatrix(mTextureMatrix);
    }
    if (mShader.flags() & Shaders::PhongGL::Flag::AlphaMask) {
        mShader.setAlphaMask(mAlphaMask);
    }
    if (mShader.flags() & Shaders::PhongGL::Flag::DoubleSided) {
        GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
    }

    mShader.draw(mMesh);

    if (mShader.flags() & Shaders::PhongGL::Flag::DoubleSided) {
        GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
    }
}

LightDrawable::LightDrawable(Object3D& object,
                             bool directional,
                             std::vector<Vector4>& positions,
                             SceneGraph::DrawableGroup3D& group)
    : SceneGraph::Drawable3D{object, &group}, mDirectional{directional}, mPositions(positions) {}

void LightDrawable::draw(const Matrix4& transformationMatrix, SceneGraph::Camera3D&) {
    mPositions.push_back(mDirectional ? Vector4{transformationMatrix.backward(), 0.0f}
                                      : Vector4{transformationMatrix.translation(), 1.0f});
}

}  // namespace kitgui