#include "gfx/gltfScene.h"

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
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/GenerateIndices.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/AnimationData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>

#include <cassert>
#include <chrono>
#include <format>
#include <memory>
#include <optional>
#include <string>

#include "gfx/drawables.h"
#include "gfx/lights.h"
#include "gfx/materials.h"
#include "gfx/sceneGraph.h"

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace Magnum::Math::Literals::ColorLiterals;

namespace {}  // namespace

namespace kitgui {
struct GltfSceneImpl {
    void Load(Magnum::Trade::AbstractImporter& importer, std::string_view debugName);
    void Draw();

    void updateLightColorBrightness();

    std::vector<MeshInfo> mMeshes;
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

GltfScene::GltfScene() : mImpl(std::make_unique<GltfSceneImpl>()) {}
GltfScene::~GltfScene() = default;

void GltfScene::Load(Magnum::Trade::AbstractImporter& importer, std::string_view debugName) {
    mImpl->Load(importer, debugName);
}

void GltfSceneImpl::Load(Magnum::Trade::AbstractImporter& importer, std::string_view debugName) {
    /* Load all textures. Textures that fail to load will be NullOpt. */
    mMaterialCache.LoadTextures(importer);

    /* Load all lights. Lights that fail to load will be NullOpt, saving the
       whole imported data so we can populate the selection info later. */
    Debug{} << "Loading" << importer.lightCount() << "lights";
    mLightCache.mLights.clear();
    mLightCache.mLights.reserve(importer.lightCount());
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
        mLightCache.mLights.push_back(std::move(lightInfo));
    }

    /* Load all materials. Materials that fail to load will be NullOpt. The
       data will be stored directly in objects later, so save them only
       temporarily. */
    mMaterialCache.LoadMaterials(importer);

    /* Load all meshes. Meshes that fail to load will be NullOpt. Remember
       which have vertex colors, so in case there's no material we can use that
       instead. */
    Debug{} << "Loading" << importer.meshCount() << "meshes";
    mMeshes.clear();
    mMeshes.reserve(importer.meshCount());
    for (uint32_t i = 0; i != importer.meshCount(); ++i) {
        mMeshes.push_back({});
        MeshInfo& mesh = mMeshes.back();
        auto meshData = std::optional<Trade::MeshData>(importer.mesh(i));
        if (!meshData) {
            Warning{} << "Cannot load mesh" << i << importer.meshName(i);
            continue;
        }

        std::string meshName = importer.meshName(i);
        if (meshName.empty())
            meshName = std::format("{}", i);

        /* Disable warnings on custom attributes, as we printed them with
           actual string names below. Generate normals for triangle meshes
           (and don't do anything for line/point meshes, there it makes no
           sense). */
        MeshTools::CompileFlags flags = MeshTools::CompileFlag::NoWarnOnCustomAttributes;
        if ((meshData->primitive() == MeshPrimitive::Triangles ||
             meshData->primitive() == MeshPrimitive::TriangleStrip ||
             meshData->primitive() == MeshPrimitive::TriangleFan) &&
            !meshData->attributeCount(Trade::MeshAttribute::Normal) &&
            meshData->hasAttribute(Trade::MeshAttribute::Position) &&
            meshData->attributeFormat(Trade::MeshAttribute::Position) == VertexFormat::Vector3) {
            /* If the mesh is a triangle strip/fan, convert to indexed
               triangles first. If the strip/fan is indexed, we can attempt to
               generate smooth normals. If it's not, generate flat ones as
               otherwise the smoothing would be only along the strip and not at
               the seams, looking weird. */
            if (meshData->primitive() == MeshPrimitive::TriangleStrip ||
                meshData->primitive() == MeshPrimitive::TriangleFan) {
                if (meshData->isIndexed()) {
                    Debug{} << "Mesh" << meshName.c_str()
                            << "doesn't have normals, generating smooth ones using information from the index "
                               "buffer for a"
                            << meshData->primitive();
                    flags |= MeshTools::CompileFlag::GenerateSmoothNormals;
                } else {
                    Debug{} << "Mesh" << meshName.c_str() << "doesn't have normals, generating flat ones for a"
                            << meshData->primitive();
                    flags |= MeshTools::CompileFlag::GenerateFlatNormals;
                }

                meshData = MeshTools::generateIndices(*Utility::move(meshData));

                /* Otherwise prefer smooth normals, if we have an index buffer
                   telling us neighboring faces */
            } else if (meshData->isIndexed()) {
                Debug{} << "Mesh" << meshName.c_str()
                        << "doesn't have normals, generating smooth ones using information from the index buffer";
                flags |= MeshTools::CompileFlag::GenerateSmoothNormals;
            } else {
                Debug{} << "Mesh" << meshName.c_str() << "doesn't have normals, generating flat ones";
                flags |= MeshTools::CompileFlag::GenerateFlatNormals;
            }
        }

        /* Print messages about ignored attributes / levels */
        for (uint32_t j = 0; j != meshData->attributeCount(); ++j) {
            const Trade::MeshAttribute name = meshData->attributeName(j);
            if (Trade::isMeshAttributeCustom(name)) {
                if (const std::string stringName = importer.meshAttributeName(name); !stringName.empty())
                    Warning{} << "Mesh" << meshName.c_str() << "has a custom mesh attribute" << stringName.c_str()
                              << Debug::nospace << ", ignoring";
                else
                    Warning{} << "Mesh" << meshName.c_str() << "has a custom mesh attribute" << name << Debug::nospace
                              << ", ignoring";
                continue;
            }

            const VertexFormat format = meshData->attributeFormat(j);
            if (isVertexFormatImplementationSpecific(format))
                Warning{} << "Mesh" << meshName.c_str() << "has" << name << "of format" << format << Debug::nospace
                          << ", ignoring";
        }
        const uint32_t meshLevels = importer.meshLevelCount(i);
        if (meshLevels > 1) {
            Warning{} << "Mesh" << meshName.c_str() << "has" << meshLevels - 1 << "additional mesh levels, ignoring";
        }

        /* Save metadata, compile the mesh */
        mesh.hasVertexColors = meshData->hasAttribute(Trade::MeshAttribute::Color);
        mesh.attributes = meshData->attributeCount();
        mesh.vertices = meshData->vertexCount();
        mesh.size = meshData->vertexData().size();
        if (meshData->isIndexed()) {
            mesh.primitives = MeshTools::primitiveCount(meshData->primitive(), meshData->indexCount());
            mesh.size += meshData->indexData().size();
        } else {
            mesh.primitives = MeshTools::primitiveCount(meshData->primitive(), meshData->vertexCount());
        }
        /* Needed for a warning when using a mesh with no tangents with a
           normal map (as, unlike with normals, we have no builtin way to
           generate tangents right now) */
        mesh.hasTangents = meshData->hasAttribute(Trade::MeshAttribute::Tangent);
        /* Needed to decide how to visualize tangent space */
        mesh.hasSeparateBitangents = meshData->hasAttribute(Trade::MeshAttribute::Bitangent);

        mesh.objectIdCount = 0;
        if (meshData->hasAttribute(Trade::MeshAttribute::ObjectId)) {
            for (const auto id : meshData->objectIdsAsArray()) {
                mesh.objectIdCount = std::max(mesh.objectIdCount, id);
            }
        }
        mesh.mesh = MeshTools::compile(*meshData, flags);
        mesh.debugName = meshName;
    }

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
            objectInfo.debugName = std::format("object {}", objectId);
        }
    }

    /* Assign parent references, separately because there's no guarantee
       that a parent was allocated already when it's referenced */
    for (const auto& parent : parents) {
        const uint32_t objectId = parent.first();
        auto& objectInfo = mSceneObjects[objectId];

        objectInfo.object->setParent(parent.second() == -1 ? &mScene : mSceneObjects[parent.second()].object);

        if (parent.second() != -1)
            ++mSceneObjects[parent.second()].childCount;
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
                if (Object3D* object = mSceneObjects[trs.first()].object)
                    (*object)
                        .setTranslation(trs.second().first())
                        .setRotation(trs.second().second())
                        .setScaling(trs.second().third());
            }
        }
        for (const auto& transformation : scene->transformations3DAsArray()) {
            if (hasTrs[transformation.first()])
                continue;
            if (Object3D* object = mSceneObjects[transformation.first()].object)
                object->setTransformation(transformation.second());
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
            if (!object)
                continue;

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
            mDrawableCache.createDrawableFromMesh(mMaterialCache, mMeshes[meshId], mSceneObjects[objectId], materialId,
                                                  mLightCache.mLightCount, mLightCache.mShadeless);
        }
    }

    /* Initialize light colors for all instantiated shaders */
    const auto lightColorsBrightness = mLightCache.calculateLightColors();
    mDrawableCache.setLightColors(lightColorsBrightness);

    /* Basic camera setup */
    (*(mCamera = new SceneGraph::Camera3D{*mCameraObject}))
        .setAspectRatioPolicy(SceneGraph::AspectRatioPolicy::Extend)
        .setProjectionMatrix(Matrix4::perspectiveProjection(75.0_degf, 1.0f, 0.01f, 1000.0f))
        .setViewport(GL::defaultFramebuffer.viewport().size());

    /* Use the settings with parameters of the camera in the model, if any,
       otherwise just used the hardcoded setup from above */
    if (importer.cameraCount()) {
        auto camera = std::optional<Trade::CameraData>(importer.camera(0));
        if (camera && camera->type() == Trade::CameraType::Perspective3D)
            mCamera->setProjectionMatrix(
                Matrix4::perspectiveProjection(camera->fov(), 1.0f, camera->near(), camera->far()));
    }

    /* Import animations */
    if (importer.animationCount())
        Debug{} << "Importing the first animation out of" << importer.animationCount();
    for (uint32_t i = 0; i != importer.animationCount(); ++i) {
        auto animation = importer.animation(i);
        if (!animation) {
            Warning{} << "Cannot load animation" << i << importer.animationName(i);
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
void GltfScene::Draw() {
    mImpl->Draw();
}

void GltfSceneImpl::Draw() {
    /* Another FB could be bound from a depth / object ID read (moreover with
       color output disabled), set it back to the default framebuffer */
    GL::defaultFramebuffer.bind(); /** @todo mapForDraw() should bind implicitly */
    GL::defaultFramebuffer.mapForDraw({{Shaders::PhongGL::ColorOutput, GL::DefaultFramebuffer::DrawAttachment::Back}})
        .clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);

    mPlayer.advance(std::chrono::system_clock::now().time_since_epoch());

    /* Calculate light positions first, upload them to all shaders -- all
       of them are there only if they are actually used, so it's not doing
       any wasteful work */
    mLightCache.mLightPositions.clear();
    mCamera->draw(mLightDrawables);
    mDrawableCache.setLightPositions(mLightCache.mLightPositions);

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