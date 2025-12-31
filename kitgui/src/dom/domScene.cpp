#include "kitgui/domScene.h"

#include "gfx/scene.h"

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