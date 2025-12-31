#include "kitgui/domScene.h"

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

#include "gfx/scene.h"
#include "kitgui/context.h"
#include "fileContext.h"
#include "log.h"

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace Magnum::Math::Literals::ColorLiterals;

namespace kitgui {
DomScene::DomScene(kitgui::Context& mContext) : mImpl(std::make_unique<DomSceneImpl>(mContext)) {}
DomScene::~DomScene() = default;

std::shared_ptr<DomScene> DomScene::Create(kitgui::Context& mContext) {
    return std::shared_ptr<DomScene>(new DomScene(mContext));
}

void DomScene::Load() {
    // TODO: we gotta reload if the scenePath changes (or, maybe, if we're fancy, inotify....)
    mImpl->Load(mProps.scenePath);
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

void DomScene::Draw() {
    mImpl->Draw();
}

}  // namespace kitgui