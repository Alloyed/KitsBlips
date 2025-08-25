#include "kitgui/context.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Platform/GLContext.h>
#include <memory>
#include <utility>
#include "kitgui/kitgui.h"

#if KITGUI_USE_SDL
#include "impl/sdl/contextImpl.h"
#elif KITGUI_USE_WIN32
#include "impl/win32/contextImpl.h"
#endif

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

bool Context::Create(kitgui::WindowApi api, bool isFloating) {
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
bool Context::SetParent(const kitgui::WindowRef& window) {
    return mImpl->SetParent(window);
}
bool Context::SetTransient(const kitgui::WindowRef& window) {
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

bool Context::IsApiSupported(kitgui::WindowApi api, bool isFloating) {
    return Impl::IsApiSupported(api, isFloating);
}

bool Context::GetPreferredApi(kitgui::WindowApi& apiOut, bool& isFloatingOut) {
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