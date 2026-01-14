#include "kitgui/context.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include "fileContext.h"
#include "theme/everforest.h"

#if KITGUI_USE_PUGL
#include "impl/pugl/contextImpl.h"
#elif KITGUI_USE_SDL
#include "impl/sdl/contextImpl.h"
#elif KITGUI_USE_WIN32
#include "impl/win32/contextImpl.h"
#elif KITGUI_USE_COCOA
#include "impl/cocoa/contextImpl.h"
#endif

namespace kitgui {
Context::~Context() {
    Destroy();
};

void Context::init(kitgui::WindowApi api, std::string_view appName) {
    Impl::init(api, appName);
}

void Context::deinit() {
    Impl::deinit();
}

Context::Context(Context::AppFactory fn)
    : mCreateAppFn(std::move(fn)),
      mFileContext(std::make_unique<FileContext>()),
      mImpl(std::make_unique<Impl>(*this)),
      mApp(nullptr) {}

bool Context::Create(bool isFloating) {
    mSizeConfigChanged = false;
    if (!mImpl->Create(isFloating)) {
        return false;
    }
    // Setup app
    mApp = mCreateAppFn(*this);
    if (mSizeConfigChanged) {
        SetSizeDirectly(mSizeConfig.startingWidth, mSizeConfig.startingHeight, mSizeConfig.resizable);
    }

    return true;
}

bool Context::Destroy() {
    if (!mImpl->IsCreated()) {
        // nothing to do
        return true;
    }
    MakeCurrent();
    mApp.reset();

    return mImpl->Destroy();
}

bool Context::IsCreated() const {
    return mImpl->IsCreated();
}

void Context::SetClearColor(Magnum::Color4 color) {
    return mImpl->SetClearColor(color);
}

FileContext* Context::GetFileContext() const {
    return mFileContext.get();
}

void Context::SetFileLoader(Context::FileLoader fn) {
    return mFileContext->SetFileLoader(std::move(fn));
}

bool Context::SetScale(double scale) {
    return mImpl->SetScale(scale);
}
const SizeConfig& Context::GetSizeConfig() const {
    return mSizeConfig;
}
void Context::SetSizeConfig(const SizeConfig& cfg) {
    mSizeConfig = cfg;
    mSizeConfigChanged = true;
}
bool Context::GetSize(uint32_t& widthOut, uint32_t& heightOut) const {
    return mImpl->GetSize(widthOut, heightOut);
}
bool Context::SetSizeDirectly(uint32_t width, uint32_t height, bool resizable) {
    return mImpl->SetSizeDirectly(width, height, resizable);
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

bool Context::NeedsUpdateLoopIntegration() {
    return Impl::NeedsUpdateLoopIntegration();
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
    SetupImGuiFont_everforest();
    SetupImGuiStyle_everforest();
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