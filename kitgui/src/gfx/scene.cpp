#include "kitgui/gfx/scene.h"

// IWYU pragma: begin_keep
#include <Corrade/Containers/OptionalStl.h>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/StringStl.h>
// IWYU pragma: end_keep

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
#include <optional>
#include <string>
#include "Magnum/Magnum.h"
#include "fileContext.h"
#include "gfx/animations.h"
#include "gfx/drawables.h"
#include "gfx/lights.h"
#include "gfx/materials.h"
#include "gfx/meshes.h"
#include "gfx/sceneGraph.h"
#include "kitgui/context.h"
#include "kitgui/types.h"
#include "log.h"

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace Magnum::Math::Literals::ColorLiterals;

namespace {
PluginManager::Manager<Trade::AbstractImporter> sImporterManager{};
}

namespace kitgui {

struct Scene::Impl {
   public:
    explicit Impl(kitgui::Context& mContext) : mContext(mContext) {};
    ~Impl() = default;
    void Load(std::string_view path);
    void LoadImpl(Magnum::Trade::AbstractImporter& importer, std::string_view debugName);
    void Update();
    void Draw();
    void SetViewport(const kitgui::Vector2& size);
    void SetBrightness(std::optional<float> brightness);

    void PlayAnimationByName(std::string_view name);
    void SetObjectRotationByName(std::string_view name, float angleRadians);
    std::optional<Vector2> GetObjectScreenPositionByName(std::string_view name);

   public:
    kitgui::Context& mContext;
    bool mLoaded = false;
    bool mLoadError = false;

    MeshCache mMeshCache;
    Magnum::SceneGraph::DrawableGroup3D mLightDrawables;

    MaterialCache mMaterialCache;
    LightCache mLightCache;
    DrawableCache mDrawableCache;

    Scene3D mScene;
    std::vector<ObjectInfo> mSceneObjects;
    Object3D* mCameraObject{};
    Magnum::SceneGraph::Camera3D* mCamera;
    std::optional<Magnum::Trade::CameraData> mCameraData;
    AnimationCache mAnimationCache;
    std::vector<AnimationPlayer*> mAdvancingAnimations;
};

Scene::Scene(kitgui::Context& mContext) : mImpl(std::make_unique<Scene::Impl>(mContext)) {};
Scene::~Scene() = default;

void Scene::Load(std::string_view path) {
    mImpl->Load(path);
}

void Scene::Impl::Load(std::string_view path) {
    if (mLoaded || mLoadError) {
        return;
    }
    Corrade::Containers::Pointer<Magnum::Trade::AbstractImporter> importer =
        sImporterManager.loadAndInstantiate("GltfImporter");
    assert(!!importer);  // FIXME: this is null right now, investigate

    const auto fileCallback = [](const std::string& filename, Magnum::InputFileCallbackPolicy policy,
                                 void* ctx) -> Containers::Optional<Containers::ArrayView<const char>> {
        std::string* fileData = ((kitgui::FileContext*)ctx)->GetOrLoadFileByName(filename, policy);
        if (fileData == nullptr) {
            return {};
        }
        return Containers::ArrayView<const char>(fileData->data(), fileData->size());
    };

    importer->setFileCallback(fileCallback, (void*)mContext.GetFileContext());
    bool success = importer->openFile({path.data(), path.size()});

    if (!success) {
        mLoadError = true;
        return;
    }
    LoadImpl(*(importer.get()), path);
    mLoaded = true;
}

void Scene::Impl::LoadImpl(Magnum::Trade::AbstractImporter& importer, std::string_view debugName) {
    kitgui::log::TimeRegion r1("Scene::Impl::LoadImpl()");

    {
        kitgui::log::TimeRegion r2("Scene::Impl::LoadImpl()::mMaterialCache");
        mMaterialCache.LoadTextures(importer);
        mMaterialCache.LoadMaterials(importer);
    }
    mLightCache.LoadLights(importer);
    mMeshCache.LoadMeshes(importer);

    /* Load the scene. Save the object pointers in an array for easier mapping
       of animations later. */
    uint32_t id = importer.defaultScene() == -1 ? 0 : importer.defaultScene();
    Debug{} << "Loading " << debugName.data() << "scene:" << id << importer.sceneName(id);

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
        objectInfo.name = importer.objectName(objectId);
        if (objectInfo.name.empty()) {
            objectInfo.name = fmt::format("{}/object:{}", debugName, objectId);
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
    mCamera = new SceneGraph::Camera3D{*mCameraObject};
    mCamera->setAspectRatioPolicy(SceneGraph::AspectRatioPolicy::Extend);
    if (importer.cameraCount()) {
        mCameraData = std::optional<Trade::CameraData>(importer.camera(0));
    }

    /* Create default camera-relative lights in case they weren't present in
       the scene already. Don't add any visualization for those. */
    mLightCache.CreateDefaultLightsIfNecessary(mCameraObject, mLightDrawables);

    /* Add drawables for objects that have a mesh, again ignoring objects
       that are not part of the hierarchy. There can be multiple mesh
       assignments for one object, simply add one drawable for each. */
    if (scene->hasField(Trade::SceneField::Mesh)) {
        // TODO: can we redo this if shadeless changes?
        for (const auto& meshMaterial : scene->meshesMaterialsAsArray()) {
            const uint32_t objectId = meshMaterial.first();
            const uint32_t meshId = meshMaterial.second().first();
            const int32_t materialId = meshMaterial.second().second();
            mDrawableCache.CreateDrawableFromMesh(mMaterialCache, mMeshCache.mMeshes[meshId], mSceneObjects[objectId],
                                                  materialId, static_cast<uint32_t>(mLightCache.mLightCount),
                                                  mLightCache.mShadeless);
        }
    }

    /* Import animations */
    mAnimationCache.LoadAnimations(importer, mSceneObjects);

    /* We need to calculate SetBrightness at least once to set up light colors/intensity */
    SetBrightness(mLightCache.mBrightness);
}

void Scene::SetViewport(const kitgui::Vector2& size) {
    mImpl->SetViewport(size);
}

void Scene::Impl::SetViewport(const kitgui::Vector2& size) {
    const Magnum::Vector2i sizei = {static_cast<int32_t>(size.x()), static_cast<int32_t>(size.y())};
    if (mCamera->viewport() == sizei) {
        return;
    }
    mCamera->setViewport(sizei);
    /* Use the settings with parameters of the camera in the model, if any,
       otherwise just use the hardcoded setup */
    float aspectRatio = size.x() / size.y();
    bool projectionFound = false;
    if (mCameraData) {
        if (mCameraData->type() == Trade::CameraType::Perspective3D) {
            mCamera->setProjectionMatrix(Matrix4::perspectiveProjection(mCameraData->fov(), aspectRatio,
                                                                        mCameraData->near(), mCameraData->far()));
            projectionFound = true;

        } else if (mCameraData->type() == Trade::CameraType::Orthographic3D) {
            mCamera->setProjectionMatrix(
                Matrix4::orthographicProjection(size, mCameraData->near(), mCameraData->far()));
            projectionFound = true;
        }
    }
    if (!projectionFound) {
        mCamera->setProjectionMatrix(Matrix4::perspectiveProjection(75.0_degf, aspectRatio, 0.01f, 1000.0f));
    }
}

void Scene::SetBrightness(std::optional<float> brightness) {
    mImpl->SetBrightness(brightness);
}

void Scene::Impl::SetBrightness(std::optional<float> brightness) {
    if(brightness) {
        mLightCache.mBrightness = *brightness;
        mLightCache.mShadeless = false;
    } else {
        mLightCache.mShadeless = true;
    }
    const auto lightColorsBrightness = mLightCache.CalculateLightColors();
    mDrawableCache.SetLightColors(lightColorsBrightness);
}


void Scene::PlayAnimationByName(std::string_view name) {
    mImpl->PlayAnimationByName(name);
}

void Scene::Impl::PlayAnimationByName(std::string_view name) {
    for (auto& info : mAnimationCache.mAnimations) {
        if (info.name == name) {
            info.player.play(std::chrono::system_clock::now().time_since_epoch());
            mAdvancingAnimations.push_back(&info.player);
        }
    }
}

void Scene::SetObjectRotationByName(std::string_view name, float angleRadians) {
    mImpl->SetObjectRotationByName(name, angleRadians);
}

void Scene::Impl::SetObjectRotationByName(std::string_view name, float angleRadians) {
    for (auto& info : mSceneObjects) {
        if (info.name == name) {
            auto rot = Quaternion::rotation(Rad(angleRadians), Vector3::yAxis());
            info.object->setRotation(rot);
        }
    }
}

std::optional<kitgui::Vector2> Scene::GetObjectScreenPositionByName(std::string_view name) {
    return mImpl->GetObjectScreenPositionByName(name);
}

std::optional<kitgui::Vector2> Scene::Impl::GetObjectScreenPositionByName(std::string_view name) {
    for (auto& info : mSceneObjects) {
        if (info.name == name) {
            const Matrix4 viewProjection =
                mCamera->projectionMatrix() * mCamera->cameraMatrix() * info.object->absoluteTransformationMatrix();
            const Vector4 clip = viewProjection * Vector4{0.0f, 0.0f, 0.0f, 1.0f};
            if (clip.w() <= 0.0f) {
                // behind camera or invalid
                return std::nullopt;
            }

            Vector3 ndc = clip.xyz() / clip.w();
            Vector2 viewportSize = Vector2{mCamera->viewport()};
            Vector2 screen;
            screen.x() = (ndc.x() * 0.5f + 0.5f) * viewportSize.x();
            screen.y() = (1.0f - (ndc.y() * 0.5f + 0.5f)) * viewportSize.y();

            return screen;
        }
    }
    return std::nullopt;
}

void Scene::Update() {
    mImpl->Update();
}
void Scene::Impl::Update() {
    for (auto& player : mAdvancingAnimations) {
        player->advance(std::chrono::system_clock::now().time_since_epoch());
    }
    // remove non-advancing animations
    mAdvancingAnimations.erase(
        std::remove_if(mAdvancingAnimations.begin(), mAdvancingAnimations.end(),
                       [](auto& player) { return player->state() == Magnum::Animation::State::Stopped; }),
        mAdvancingAnimations.end());
}

void Scene::Draw() {
    mImpl->Draw();
}
void Scene::Impl::Draw() {
    if (!mLoaded) {
        return;
    }
    /* Another FB could be bound from a depth / object ID read (moreover with
       color output disabled), set it back to the default framebuffer */
    GL::defaultFramebuffer.bind(); /** @todo mapForDraw() should bind implicitly */
    GL::defaultFramebuffer.mapForDraw({{Shaders::PhongGL::ColorOutput, GL::DefaultFramebuffer::DrawAttachment::Back}});
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);

    auto size = GL::defaultFramebuffer.viewport().size();
    const Magnum::Vector2 sizef = {static_cast<float>(size.x()), static_cast<float>(size.y())};
    SetViewport(sizef);

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