#include <imgui.h>
#include <memory>
#include "imgui/imguiHelpers.h"
#include "imgui/knob.h"
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"
#include "scene3d.h"

class MyApp : public kitgui::BaseApp {
   public:
    MyApp(kitgui::Context& mContext) : kitgui::BaseApp(mContext), mScene() {}
    ~MyApp() = default;

   protected:
    void OnUpdate() override {
        kitgui::helpers::beginFullscreen([&]() {
            ImGui::Text("Oh yeah, gamer time!");
            kitgui::knob("Rise", {}, mKnob);
        });
    }

    void OnDraw(const GladGLContext& gl) override {
        mScene.Bind(gl);
        mScene.Draw(gl);
    }

   private:
    kitgui::Scene3d mScene;
    double mKnob = 0.0;
};

int main() {
    kitgui::init();

    kitgui::Context ctx1{};
    ctx1.SetApp(std::make_shared<MyApp>(ctx1));
    ctx1.Create(kitgui::platform::Api::Any, true);
    ctx1.SetSize(400, 400);
    ctx1.SetClearColor({0.3f, 0.7f, 0.3f, 1.0f});

    kitgui::Context ctx2{};
    ctx2.SetApp(std::make_shared<MyApp>(ctx2));
    ctx2.Create(kitgui::platform::Api::Any, true);
    ctx2.SetSize(400, 400);
    ctx2.SetClearColor({0.3f, 0.3f, 0.3f, 1.0f});

    ctx1.Show();
    ctx2.Show();

    kitgui::Context::RunLoop();

    // update loop here
    kitgui::deinit();
}