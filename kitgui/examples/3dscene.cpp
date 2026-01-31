#include <Magnum/Math/Color.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <memory>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/gfx/scene.h"
#include "kitgui/kitgui.h"
#include "kitgui/types.h"

class MyApp : public kitgui::BaseApp {
   public:
    explicit MyApp(kitgui::Context& mContext)
        : kitgui::BaseApp(mContext), mScene(std::make_unique<kitgui::Scene>(mContext)) {}
    ~MyApp() = default;

   protected:
    void OnActivate() override {
        mScene->Load(mFilePath);
        GetContext().SetClearColor(mClearColor);
    }
    void OnUpdate() override {
        mScene->Update();
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

                if(ImGui::DragFloat("brightness", &mBrightness, 0.0001, 0.0f, 1.0f, "%.6f")) {
                    mScene->SetBrightness(mBrightness);
                }

                if (ImGui::ColorEdit4("bgcolor", reinterpret_cast<float*>(&mClearColor))) {
                    GetContext().SetClearColor(mClearColor);
                }
            }
            ImGui::End();
        }
    }
    void OnDraw() override { mScene->Draw(); }

   private:
    std::unique_ptr<kitgui::Scene> mScene;
    bool mShowUi = true;
    std::string mFilePath{PROJECT_DIR "/../daw/assets/kitskeys.glb"};
    kitgui::Color4 mClearColor{0.1f, 0.4f, 0.4f, 1.0f};
    float mBrightness = 0.0025f;
};

int main() {
    kitgui::Context::init(kitgui::WindowApi::Any, "kitgui");

    {
        kitgui::Context ctx1([](kitgui::Context& ctx) { return std::make_unique<MyApp>(ctx); });
        ctx1.SetSizeConfig({600, 400, true, false});
        ctx1.Create(true);
        ctx1.Show();

        kitgui::Context::RunLoop();
    }

    kitgui::Context::deinit();
}
