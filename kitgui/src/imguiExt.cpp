#if 0
#include "clapeze/gui/imguiExt.h"

#include "clapeze/ext/gui.h"
#include "clapeze/gui/sdlOpenGlExt.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

namespace clapeze {

bool ImGuiExt::Create(ClapWindowApi api, bool isFloating) {
    if (!SdlOpenGlExt::Create(api, isFloating)) {
        return false;
    }

    // setup imgui
    IMGUI_CHECKVERSION();
    mImgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(mImgui);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(mWindow, mCtx);
    ImGui_ImplOpenGL3_Init();

    return true;
}

void ImGuiExt::Destroy() {
    ImGui::SetCurrentContext(mImgui);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(mImgui);
    mImgui = nullptr;
    SdlOpenGlExt::Destroy();
}

bool ImGuiExt::MakeCurrent() {
    if (mImgui) {
        ImGui::SetCurrentContext(mImgui);
    }
    return SdlOpenGlExt::MakeCurrent();
}

void ImGuiExt::OnEvent(const SDL_Event& event) {
    ImGui_ImplSDL3_ProcessEvent(&event);  // Forward your event to backend
}

void ImGuiExt::Update() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    mConfig.onGui();

    ImGui::Render();
}

void ImGuiExt::Draw() {
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
}  // namespace clapeze
#endif