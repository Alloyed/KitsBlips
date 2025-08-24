#include "kitgui/context.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Platform/GLContext.h>
#include <memory>
#include <utility>
#include "kitgui/kitgui.h"

// TODO: swap out
#include "impl/sdl/contextImpl.h"

using namespace Magnum;

namespace kitgui {
Context::~Context() = default;
void Context::init() {
    Impl::init();
}

void Context::deinit() {
    Impl::deinit();
}

Context::Context(Context::AppFactory fn) : mCreateAppFn(std::move(fn)), mImpl(std::make_unique<Impl>(*this)) {}

bool Context::Create(platform::Api api, bool isFloating) {
    if (!mImpl->Create(api, isFloating)) {
        return false;
    }
    // Setup app
    mApp = mCreateAppFn(*this);

    return true;
}

bool Context::Destroy() {
    mApp.reset();

    return mImpl->Destroy();
}

bool Context::IsCreated() const {
    return mImpl->IsCreated();
}

bool Context::SetScale(double scale) {
    return mImpl->SetScale(scale);
}
const SizeConfig& Context::GetSizeConfig() const {
    return mSizeConfig;
}
void Context::SetSizeConfig(const SizeConfig& cfg) {
    mSizeConfig = cfg;
}
bool Context::GetSize(uint32_t& widthOut, uint32_t& heightOut) const {
    return mImpl->GetSize(widthOut, heightOut);
}
bool Context::SetSizeDirectly(uint32_t width, uint32_t height) {
    return mImpl->SetSizeDirectly(width, height);
}
bool Context::SetParent(const platform::WindowRef& window) {
    return mImpl->SetParent(window);
}
bool Context::SetTransient(const platform::WindowRef& window) {
    return mImpl->SetTransient(window);
}
void Context::SuggestTitle(std::string_view title) {
    return mImpl->SuggestTitle(title);
}
bool Context::Show() {
    return mImpl->Show();
}
bool Context::Hide() {
    return mImpl->Hide();
}
bool Context::Close() {
    return mImpl->Close();
}

void Context::MakeCurrent() {
    mImpl->MakeCurrent();
}

void Context::RunLoop() {
    Impl::RunLoop();
}

void Context::RunSingleFrame() {
    Impl::RunSingleFrame();
}

bool Context::GetPreferredApi(platform::Api& apiOut, bool& isFloatingOut) {
    return Impl::GetPreferredApi(apiOut, isFloatingOut);
}

void Context::OnActivate() {
    if (mApp) {
        mApp->OnActivate();
    }
}

void Context::OnDeactivate() {
    if (mApp) {
        mApp->OnDeactivate();
    }
}

void Context::OnUpdate() {
    if (mApp) {
        mApp->OnUpdate();
    }
}

void Context::OnDraw() {
    if (mApp) {
        mApp->OnDraw();
    }
}

}  // namespace kitgui