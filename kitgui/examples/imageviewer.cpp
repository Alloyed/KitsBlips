#include <Magnum/Math/Color.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <memory>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/gfx/image.h"
#include "kitgui/kitgui.h"
#include "kitgui/types.h"

namespace {
static const char* sFile = PROJECT_DIR "/assets/ham.png";
}

class MyApp : public kitgui::BaseApp {
   public:
    explicit MyApp(kitgui::Context& mContext)
        : kitgui::BaseApp(mContext), mImage(std::make_unique<kitgui::Image>(mContext)), mFilePath(sFile) {}
    ~MyApp() = default;

   protected:
    void OnActivate() override {
        mImage->Load(mFilePath);
        GetContext().SetClearColor(mClearColor);
    }
    void OnUpdate() override {
        uint32_t w{};
        uint32_t h{};
        GetContext().GetSize(w, h);
        ImGui::ImageWithBg(mImage->GetId(), ImVec2(w, h));
    }
    void OnGuiUpdate() override {
        if (mShowUi) {
            ImGui::SetNextWindowPos({ImGui::GetWindowWidth() - 300, ImGui::GetWindowHeight() - 200}, ImGuiCond_Once);
            ImGui::SetNextWindowSize({300, 200}, ImGuiCond_Once);
            if (ImGui::Begin("Viewer", &mShowUi)) {
                ImGui::InputText("file", &mFilePath);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    mImage.reset();
                    mImage = std::make_unique<kitgui::Image>(GetContext());
                    mImage->Load(mFilePath);
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

            }
            ImGui::End();
        }
    }
    void OnDraw() override {}

   private:
    std::unique_ptr<kitgui::Image> mImage;
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
