#include "kitgui/dom.h"

// enable conversion to stl
#include <Corrade/Containers/OptionalStl.h>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/StringStl.h>
// done

// Corrade is magnum's internal "base layer". where possible replace with stl and etl
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/BitArray.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StridedArrayView.h>
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
#include <memory>
#include <optional>
#include <string>

#include "gfx/drawables.h"
#include "gfx/lights.h"
#include "gfx/materials.h"
#include "gfx/meshes.h"
#include "gfx/sceneGraph.h"
#include "kitgui/dom.h"
#include "log.h"

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace Magnum::Math::Literals::ColorLiterals;

namespace kitgui {
struct DomSceneImpl {
    void Load(Magnum::Trade::AbstractImporter& importer, std::string_view debugName);
    void Update();
    void Draw();

    MeshCache mMeshCache;
    Magnum::SceneGraph::DrawableGroup3D mLightDrawables;

    MaterialCache mMaterialCache;
    LightCache mLightCache;
    DrawableCache mDrawableCache;

    Scene3D mScene;
    std::vector<ObjectInfo> mSceneObjects;
    Object3D* mCameraObject{};
    SceneGraph::Camera3D* mCamera;

    Animation::Player<std::chrono::nanoseconds, float> mPlayer;
};

DomScene::DomScene() : mImpl(std::make_unique<DomSceneImpl>()) {}
DomScene::~DomScene() = default;

std::shared_ptr<DomScene> DomScene::Create() {
    return std::shared_ptr<DomScene>(new DomScene());
}

void DomScene::Load() {
    // TODO: abstractImporter
    // mImpl->Load(importer, debugName);
}

void DomSceneImpl::Load(Magnum::Trade::AbstractImporter& importer, std::string_view debugName) {
    mMaterialCache.LoadTextures(importer);
    mMaterialCache.LoadMaterials(importer);
    mLightCache.LoadLights(importer);
    mMeshCache.LoadMeshes(importer);

    /* Load the scene. Save the object pointers in an array for easier mapping
       of animations later. */
    uint32_t id = importer.defaultScene() == -1 ? 0 : importer.defaultScene();
    Debug{} << "Loading scene" << id << importer.sceneName(id);

    auto scene = std::optional<Trade::SceneData>(importer.scene(id));
    if (!scene || !scene->is3D() || !scene->hasField(Trade::SceneField::Parent)) {
        Error{} << "Cannot load the scene, aborting";
        return;
    }

    /* Allocate objects that are part of the hierarchy and fill their
       implicit info */
    mSceneObjects.clear();
    mSceneObjects.resize(scene->mappingBound());
    const auto parents = scene->parentsAsArray();
    for (const auto& parent : parents) {
        const uint32_t objectId = parent.first();
        auto& objectInfo = mSceneObjects[objectId];

        objectInfo.id = objectId;
        objectInfo.object = new Object3D{};
        objectInfo.debugName = importer.objectName(objectId);
        if (objectInfo.debugName.empty()) {
            objectInfo.debugName = fmt::format("object {}", objectId);
        }
    }

    /* Assign parent references, separately because there's no guarantee
       that a parent was allocated already when it's referenced */
    for (const auto& parent : parents) {
        const uint32_t objectId = parent.first();
        auto& objectInfo = mSceneObjects[objectId];

        objectInfo.object->setParent(parent.second() == -1 ? &mScene : mSceneObjects[parent.second()].object);
    }

    /* Set transformations. Objects that are not part of the hierarchy are
       ignored, objects that have no transformation entry retain an
       identity transformation. Assign TRS first, if available, and then
       fall back to matrices for the rest. */
    {
        Containers::BitArray hasTrs{ValueInit, std::size_t(scene->mappingBound())};
        if (scene->hasField(Trade::SceneField::Translation) || scene->hasField(Trade::SceneField::Rotation) ||
            scene->hasField(Trade::SceneField::Scaling)) {
            for (const auto& trs : scene->translationsRotationsScalings3DAsArray()) {
                hasTrs.set(trs.first());
                if (Object3D* object = mSceneObjects[trs.first()].object) {
                    (*object)
                        .setTranslation(trs.second().first())
                        .setRotation(trs.second().second())
                        .setScaling(trs.second().third());
                }
            }
        }
        for (const auto& transformation : scene->transformations3DAsArray()) {
            if (hasTrs[transformation.first()]) {
                continue;
            }
            if (Object3D* object = mSceneObjects[transformation.first()].object) {
                object->setTransformation(transformation.second());
            }
        }
    }

    /* Import all lights so we know which shaders to instantiate */
    mLightCache.CreateSceneLights(*scene, mSceneObjects, mLightDrawables);

    /* Import camera references, the first camera will be treated as the
       default one */
    if (scene->hasField(Trade::SceneField::Camera)) {
        for (const auto& cameraReference : scene->camerasAsArray()) {
            const uint32_t objectId = cameraReference.first();
            const auto& objectInfo = mSceneObjects[objectId];
            Object3D* const object = objectInfo.object;
            if (!object) {
                continue;
            }

            if (cameraReference.second() == 0) {
                mCameraObject = object;
            }
        }
    }

    /* Create a camera object in case it wasn't present in the scene already */
    if (!mCameraObject) {
        mCameraObject = new Object3D{&mScene};
        mCameraObject->translate(Vector3::zAxis(5.0f));
    }

    /* Create default camera-relative lights in case they weren't present in
       the scene already. Don't add any visualization for those. */
    mLightCache.CreateDefaultLightsIfNecessary(mCameraObject, mLightDrawables);

    /* Add drawables for objects that have a mesh, again ignoring objects
       that are not part of the hierarchy. There can be multiple mesh
       assignments for one object, simply add one drawable for each. */
    if (scene->hasField(Trade::SceneField::Mesh)) {
        for (const auto& meshMaterial : scene->meshesMaterialsAsArray()) {
            const uint32_t objectId = meshMaterial.first();
            const uint32_t meshId = meshMaterial.second().first();
            const int32_t materialId = meshMaterial.second().second();
            mDrawableCache.CreateDrawableFromMesh(mMaterialCache, mMeshCache.mMeshes[meshId], mSceneObjects[objectId],
                                                  materialId, mLightCache.mLightCount, mLightCache.mShadeless);
        }
    }

    /* Initialize light colors for all instantiated shaders */
    const auto lightColorsBrightness = mLightCache.CalculateLightColors();
    mDrawableCache.SetLightColors(lightColorsBrightness);

    /* Basic camera setup */
    (*(mCamera = new SceneGraph::Camera3D{*mCameraObject}))
        .setAspectRatioPolicy(SceneGraph::AspectRatioPolicy::Extend)
        .setProjectionMatrix(Matrix4::perspectiveProjection(75.0_degf, 1.0f, 0.01f, 1000.0f))
        .setViewport(GL::defaultFramebuffer.viewport().size());

    /* Use the settings with parameters of the camera in the model, if any,
       otherwise just used the hardcoded setup from above */
    if (importer.cameraCount()) {
        auto camera = std::optional<Trade::CameraData>(importer.camera(0));
        if (camera && camera->type() == Trade::CameraType::Perspective3D) {
            mCamera->setProjectionMatrix(
                Matrix4::perspectiveProjection(camera->fov(), 1.0f, camera->near(), camera->far()));
        }
    }

    /* Import animations */
    if (importer.animationCount()) {
        kitgui::log::info("importing animations");
    }
    for (uint32_t i = 0; i != importer.animationCount(); ++i) {
        auto animation = importer.animation(i);
        if (!animation) {
            kitgui::log::error(fmt::format("cannot load animation {} {}", i, importer.animationName(i)));
            continue;
        }

        for (uint32_t j = 0; j != animation->trackCount(); ++j) {
            if (animation->trackTarget(j) >= mSceneObjects.size() || !mSceneObjects[animation->trackTarget(j)].object) {
                continue;
            }

            Object3D& animatedObject = *mSceneObjects[animation->trackTarget(j)].object;

            if (animation->trackTargetName(j) == Trade::AnimationTrackTarget::Translation3D) {
                const auto callback = [](float, const Vector3& translation, Object3D& object) {
                    object.setTranslation(translation);
                };
                if (animation->trackType(j) == Trade::AnimationTrackType::CubicHermite3D) {
                    mPlayer.addWithCallback(animation->track<CubicHermite3D>(j), callback, animatedObject);
                } else {
                    assert(animation->trackType(j) == Trade::AnimationTrackType::Vector3);
                    mPlayer.addWithCallback(animation->track<Vector3>(j), callback, animatedObject);
                }
            } else if (animation->trackTargetName(j) == Trade::AnimationTrackTarget::Rotation3D) {
                const auto callback = [](float, const Quaternion& rotation, Object3D& object) {
                    object.setRotation(rotation);
                };
                if (animation->trackType(j) == Trade::AnimationTrackType::CubicHermiteQuaternion) {
                    mPlayer.addWithCallback(animation->track<CubicHermiteQuaternion>(j), callback, animatedObject);
                } else {
                    assert(animation->trackType(j) == Trade::AnimationTrackType::Quaternion);
                    mPlayer.addWithCallback(animation->track<Quaternion>(j), callback, animatedObject);
                }
            } else if (animation->trackTargetName(j) == Trade::AnimationTrackTarget::Scaling3D) {
                const auto callback = [](float, const Vector3& scaling, Object3D& object) {
                    object.setScaling(scaling);
                };
                if (animation->trackType(j) == Trade::AnimationTrackType::CubicHermite3D) {
                    mPlayer.addWithCallback(animation->track<CubicHermite3D>(j), callback, animatedObject);
                } else {
                    assert(animation->trackType(j) == Trade::AnimationTrackType::Vector3);
                    mPlayer.addWithCallback(animation->track<Vector3>(j), callback, animatedObject);
                }
            } else {
            }
        }
        //_data->animationData = animation->release();

        /* Load only the first animation at the moment */
        break;
    }
}

const DomScene::Props& DomScene::GetProps() const {
    return mProps;
}

void DomScene::SetProps(const DomScene::Props& props) {
    // TODO: dirty flagging
    mProps = props;
}

void DomScene::Update() {
    mImpl->Update();
}

void DomSceneImpl::Update() {
    mPlayer.advance(std::chrono::system_clock::now().time_since_epoch());
}

void DomScene::Draw() {
    mImpl->Draw();
}

void DomSceneImpl::Draw() {
    /* Another FB could be bound from a depth / object ID read (moreover with
       color output disabled), set it back to the default framebuffer */
    GL::defaultFramebuffer.bind(); /** @todo mapForDraw() should bind implicitly */
    GL::defaultFramebuffer.mapForDraw({{Shaders::PhongGL::ColorOutput, GL::DefaultFramebuffer::DrawAttachment::Back}})
        .clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);

    /* Calculate light positions first, upload them to all shaders -- all
       of them are there only if they are actually used, so it's not doing
       any wasteful work */
    mLightCache.mLightPositions.clear();
    mCamera->draw(mLightDrawables);
    mDrawableCache.SetLightPositions(mLightCache.mLightPositions);

    /* Draw opaque stuff as usual */
    mCamera->draw(mDrawableCache.mOpaqueDrawables);

/* Draw transparent stuff back-to-front with blending enabled */
#if 0
        if (!_data->transparentDrawables.isEmpty()) {
            GL::Renderer::setDepthMask(false);
            GL::Renderer::enable(GL::Renderer::Feature::Blending);
            /* Ugh non-premultiplied alpha */
            GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::SourceAlpha,
                                           GL::Renderer::BlendFunction::OneMinusSourceAlpha);

            std::vector<std::pair<std::reference_wrapper<SceneGraph::Drawable3D>, Matrix4>> drawableTransformations =
                _data->camera->drawableTransformations(_data->transparentDrawables);
            std::sort(drawableTransformations.begin(), drawableTransformations.end(),
                      [](const std::pair<std::reference_wrapper<SceneGraph::Drawable3D>, Matrix4>& a,
                         const std::pair<std::reference_wrapper<SceneGraph::Drawable3D>, Matrix4>& b) {
                          return a.second.translation().z() > b.second.translation().z();
                      });
            _data->camera->draw(drawableTransformations);

            GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::One, GL::Renderer::BlendFunction::Zero);
            GL::Renderer::disable(GL::Renderer::Feature::Blending);
            GL::Renderer::setDepthMask(true);
        }
#endif
}
}  // namespace kitgui