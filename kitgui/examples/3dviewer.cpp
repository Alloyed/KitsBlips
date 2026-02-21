#include <Magnum/Math/Color.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <memory>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/gfx/scene.h"
#include "kitgui/kitgui.h"
#include "kitgui/types.h"

namespace {
static const char* sFile = PROJECT_DIR "/../daw/assets/kitskeys.glb";
}

class MyApp : public kitgui::BaseApp {
   public:
    explicit MyApp(kitgui::Context& mContext)
        : kitgui::BaseApp(mContext), mScene(std::make_unique<kitgui::Scene>(mContext)), mFilePath(sFile) {}
    ~MyApp() = default;

   protected:
    void OnActivate() override {
        mScene->Load(mFilePath);
        GetContext().SetClearColor(mClearColor);
    }
    void OnUpdate() override { mScene->Update(); }
    void OnGuiUpdate() override {
        if (mShowUi) {
            ImGui::SetNextWindowPos({ImGui::GetWindowWidth() - 300, ImGui::GetWindowHeight() - 200}, ImGuiCond_Once);
            ImGui::SetNextWindowSize({300, 200}, ImGuiCond_Once);
            if (ImGui::Begin("Viewer", &mShowUi)) {
                ImGui::InputText("file", &mFilePath);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    mScene.reset();
                    mScene = std::make_unique<kitgui::Scene>(GetContext());
                    mScene->Load(mFilePath);
                }

                uint32_t w{};
                uint32_t h{};
                GetContext().GetSize(w, h);
                int32_t data[2] = {static_cast<int32_t>(w), static_cast<int32_t>(h)};
                ImGui::InputInt2("size", data);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    w = static_cast<uint32_t>(data[0]);
                    h = static_cast<uint32_t>(data[1]);
                    GetContext().SetSizeConfig({.startingWidth = w, .startingHeight = h});
                    GetContext().SetSizeDirectly(w, h, false);
                }

                bool changed = false;
                kitgui::SceneTweakables& cfg = mScene->GetSceneTweakables();
                if (ImGui::Checkbox("shadeless", &cfg.shadeless)) {
                    changed = true;
                }
                if (ImGui::DragFloat("brightness", &cfg.dynamicLightFactor, 0.0001, 0.0f, 1.0f, "%.6f")) {
                    changed = true;
                }
                if (ImGui::DragFloat("ambient brightness", &cfg.ambientLightFactor, 0.01, 0.0f, 1.0f, "%.3f")) {
                    changed = true;
                }
                if (ImGui::DragFloat("bloom", &cfg.bloomIntensity, 0.01, 0.0f, 3.0f, "%.3f")) {
                    changed = true;
                }
                if (ImGui::DragFloat("exposure", &cfg.hdrExposureFactor, 0.01, 0.0f, 10.0f, "%.3f")) {
                    changed = true;
                }
                if (ImGui::DragFloat("gamma", &cfg.gamma, 0.01, 0.0f, 10.0f, "%.3f")) {
                    changed = true;
                }
                if (changed) {
                    mScene->ApplySceneTweakables();
                }
                mScene->ImGui();
            }
            ImGui::End();
        }
    }
    void OnDraw() override { mScene->Draw(); }

   private:
    std::unique_ptr<kitgui::Scene> mScene;
    bool mShowUi = true;
    std::string mFilePath{};
    kitgui::Color4 mClearColor{0.1f, 0.4f, 0.4f, 1.0f};
};

int main(int argc, const char* argv[]) {
    kitgui::Context::init(kitgui::WindowApi::Any, "kitgui");

    if (argc >= 2) {
        sFile = argv[1];
    }

    {
        kitgui::Context ctx1([](kitgui::Context& ctx) { return std::make_unique<MyApp>(ctx); });
        ctx1.SetSizeConfig({900, 600, true, false});
        ctx1.Create(true);
        ctx1.Show();

        kitgui::Context::RunLoop();
    }

    kitgui::Context::deinit();
}
