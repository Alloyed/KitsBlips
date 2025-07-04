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
#include <Magnum/Math/CubicHermite.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/GenerateIndices.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/Shaders/FlatGL.h>
#include <Magnum/Shaders/MeshVisualizerGL.h>
#include <Magnum/Shaders/PhongGL.h>
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

namespace {
struct MeshInfo {
    std::optional<Magnum::GL::Mesh> mesh;
    uint32_t attributes;
    uint32_t vertices;
    uint32_t primitives;
    uint32_t objectIdCount;
    size_t size;
    std::string debugName;
    bool hasTangents, hasSeparateBitangents;
};

}  // namespace

namespace kitgui {
struct GltfSceneImpl {
    void Load(Magnum::Trade::AbstractImporter& importer, std::string_view debugName);

    Shaders::FlatGL3D& flatShader(Shaders::FlatGL3D::Flags flags);
    Shaders::PhongGL& phongShader(Shaders::PhongGL::Flags flags);
    void updateLightColorBrightness();

    std::vector<MeshInfo> mMeshes;
    Magnum::SceneGraph::DrawableGroup3D mOpaqueDrawables;
    Magnum::SceneGraph::DrawableGroup3D mLightDrawables;

    MaterialCache mMaterialCache;
    LightCache mLightCache;

    Scene3D mScene;
    std::vector<ObjectInfo> mSceneObjects;
    Object3D* mCameraObject{};
    SceneGraph::Camera3D* mCamera;

    Animation::Player<std::chrono::nanoseconds, float> mPlayer;

    /* Indexed by Shaders::FlatGL3D::Flags, PhongGL::Flags or
   MeshVisualizerGL3D::Flags but cast to an UnsignedInt because I
   refuse to deal with the std::hash crap. */
    std::unordered_map<UnsignedInt, Shaders::FlatGL3D> mFlatShaders;
    std::unordered_map<UnsignedInt, Shaders::PhongGL> mPhongShaders;
    std::unordered_map<UnsignedInt, Shaders::MeshVisualizerGL3D> mMeshVisualizerShaders;
};

GltfScene::GltfScene() : mImpl(std::make_unique<GltfSceneImpl>()) {}

Shaders::FlatGL3D& GltfSceneImpl::flatShader(Shaders::FlatGL3D::Flags flags) {
    auto found = mFlatShaders.find(enumCastUnderlyingType(flags));
    if (found == mFlatShaders.end()) {
        Shaders::FlatGL3D::Configuration configuration;
        configuration.setFlags(Shaders::FlatGL3D::Flag::ObjectId | flags);
        found = mFlatShaders.emplace(enumCastUnderlyingType(flags), Shaders::FlatGL3D{configuration}).first;
    }
    return found->second;
}

Shaders::PhongGL& GltfSceneImpl::phongShader(Shaders::PhongGL::Flags flags) {
    auto found = mPhongShaders.find(enumCastUnderlyingType(flags));
    if (found == mPhongShaders.end()) {
        Shaders::PhongGL::Configuration configuration;
        configuration.setFlags(Shaders::PhongGL::Flag::ObjectId | flags)
            .setLightCount(mLightCache.mLightCount ? mLightCache.mLightCount : 3);
        found = mPhongShaders.emplace(enumCastUnderlyingType(flags), Shaders::PhongGL{configuration}).first;

        found->second.setSpecularColor(0x11111100_rgbaf).setShininess(80.0f);
    }
    return found->second;
}

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
    Containers::BitArray hasVertexColors{ValueInit, importer.meshCount()};
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
        if (meshLevels > 1)
            Warning{} << "Mesh" << meshName.c_str() << "has" << meshLevels - 1 << "additional mesh levels, ignoring";

        hasVertexColors.set(i, meshData->hasAttribute(Trade::MeshAttribute::Color));

        /* Save metadata, compile the mesh */
        mesh.attributes = meshData->attributeCount();
        mesh.vertices = meshData->vertexCount();
        mesh.size = meshData->vertexData().size();
        if (meshData->isIndexed()) {
            mesh.primitives = MeshTools::primitiveCount(meshData->primitive(), meshData->indexCount());
            mesh.size += meshData->indexData().size();
        } else
            mesh.primitives = MeshTools::primitiveCount(meshData->primitive(), meshData->vertexCount());
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

    /* Add drawables for objects that have a mesh, again ignoring objects
       that are not part of the hierarchy. There can be multiple mesh
       assignments for one object, simply add one drawable for each. */
    if (scene->hasField(Trade::SceneField::Mesh)) {
        for (const auto& meshMaterial : scene->meshesMaterialsAsArray()) {
            const uint32_t objectId = meshMaterial.first();
            auto& objectInfo = mSceneObjects[objectId];
            Object3D* const object = objectInfo.object;
            const uint32_t meshId = meshMaterial.second().first();
            const Int materialId = meshMaterial.second().second();
            std::optional<GL::Mesh>& mesh = mMeshes[meshId].mesh;
            if (!object || !mesh)
                continue;

            /* Save the mesh pointer as well, so we know what to draw for object
               selection */
            objectInfo.meshId = meshId;

            Shaders::PhongGL::Flags flags;
            if (hasVertexColors[meshId])
                flags |= Shaders::PhongGL::Flag::VertexColor;
            if (mMeshes[meshId].hasSeparateBitangents)
                flags |= Shaders::PhongGL::Flag::Bitangent;

            /* Material not available / not loaded. If the mesh has vertex
               colors, use that, otherwise apply a default material; use a flat
               shader for lines / points */
            if (materialId == -1 || !mMaterialCache.mMaterials[materialId].raw.has_value()) {
                if (mesh->primitive() == GL::MeshPrimitive::Triangles ||
                    mesh->primitive() == GL::MeshPrimitive::TriangleStrip ||
                    mesh->primitive() == GL::MeshPrimitive::TriangleFan) {
                    new PhongDrawable{*object,       phongShader(flags),     *mesh,           objectId,
                                      0xffffff_rgbf, mLightCache.mShadeless, mOpaqueDrawables};
                } else {
                    new FlatDrawable{*object,
                                     flatShader((hasVertexColors[meshId] ? Shaders::FlatGL3D::Flag::VertexColor
                                                                         : Shaders::FlatGL3D::Flags{})),
                                     *mesh,
                                     objectId,
                                     0xffffff_rgbf,
                                     Vector3{Constants::nan()},
                                     mOpaqueDrawables};
                }

                /* Material available */
            } else {
                const Trade::PhongMaterialData& material = *mMaterialCache.mMaterials[materialId].Phong();

                if (material.isDoubleSided())
                    flags |= Shaders::PhongGL::Flag::DoubleSided;

                /* Textured material. If the texture failed to load, again just
                   use a default-colored material. */
                GL::Texture2D* diffuseTexture = nullptr;
                GL::Texture2D* normalTexture = nullptr;
                float normalTextureScale = 1.0f;
                if (material.hasAttribute(Trade::MaterialAttribute::DiffuseTexture)) {
                    std::optional<GL::Texture2D>& texture = mMaterialCache.mTextures[material.diffuseTexture()].texture;
                    if (texture) {
                        diffuseTexture = &*texture;
                        flags |= Shaders::PhongGL::Flag::AmbientTexture | Shaders::PhongGL::Flag::DiffuseTexture;
                        if (material.hasTextureTransformation())
                            flags |= Shaders::PhongGL::Flag::TextureTransformation;
                        if (material.alphaMode() == Trade::MaterialAlphaMode::Mask)
                            flags |= Shaders::PhongGL::Flag::AlphaMask;
                    }
                }

                /* Normal textured material. If the textures fail to load,
                   again just use a default-colored material. */
                if (material.hasAttribute(Trade::MaterialAttribute::NormalTexture)) {
                    std::optional<GL::Texture2D>& texture = mMaterialCache.mTextures[material.normalTexture()].texture;
                    /* If there are no tangents, the mesh would render all
                       black. Ignore the normal map in that case. */
                    /** @todo generate tangents instead once we have the algo */
                    if (!mMeshes[meshId].hasTangents) {
                        Warning{} << "Mesh" << mMeshes[meshId].debugName.c_str()
                                  << "doesn't have tangents and Magnum can't generate them yet, ignoring a "
                                     "normal map";
                    } else if (texture) {
                        normalTexture = &*texture;
                        normalTextureScale = material.normalTextureScale();
                        flags |= Shaders::PhongGL::Flag::NormalTexture;
                        if (material.hasTextureTransformation())
                            flags |= Shaders::PhongGL::Flag::TextureTransformation;
                    }
                }

                if (material.alphaMode() == Trade::MaterialAlphaMode::Blend) {
                    // todo, transparent materials
                    continue;
                }

                new PhongDrawable{*object,
                                  phongShader(flags),
                                  *mesh,
                                  objectId,
                                  material.diffuseColor(),
                                  diffuseTexture,
                                  normalTexture,
                                  normalTextureScale,
                                  material.alphaMask(),
                                  material.commonTextureMatrix(),
                                  mLightCache.mShadeless,
                                  mOpaqueDrawables};
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

    /* Initialize light colors for all instantiated shaders */
    const auto lightColorsBrightness = mLightCache.calculateLightColors();
    for (auto& shader : mPhongShaders) {
        shader.second.setLightColors(lightColorsBrightness);
    }

    /* Basic camera setup */
    (*(mCamera = new SceneGraph::Camera3D{*mCameraObject}))
        .setAspectRatioPolicy(SceneGraph::AspectRatioPolicy::Extend)
        .setProjectionMatrix(Matrix4::perspectiveProjection(75.0_degf, 1.0f, 0.01f, 1000.0f))
        .setViewport(GL::defaultFramebuffer.viewport().size());

    /* Use the settings with parameters of the camera in the model, if any,
       otherwise just used the hardcoded setup from above */
    if (importer.cameraCount()) {
        auto camera = std::optional<Trade::CameraData>(importer.camera(0));
        if (camera)
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
}  // namespace kitgui