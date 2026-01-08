#include <Magnum/Math/Color.h>
#include <imgui.h>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/gfx/scene.h"
#include "kitgui/immediateMode.h"
#include "kitgui/kitgui.h"

class MyApp : public kitgui::BaseApp {
   public:
    explicit MyApp(kitgui::Context& mContext) : kitgui::BaseApp(mContext), mScene(mContext) {
        mContext.SetSizeConfig({400, 400});
        mContext.SetClearColor(Magnum::Math::Color4(0.3f, 0.7f, 0.3f, 1.0f));
    }
    ~MyApp() = default;

   protected:
    void OnActivate() override {
        mScene.Load("../assets/duck.glb");
        mScene.PlayAnimationByName("Suzanne");
    }
    void OnUpdate() override {
        mScene.Update();
        kitgui::ImGuiScene("##myscene", mScene);
        ImGui::Text("Oh yeah, gamer time!");
    }

   private:
    kitgui::Scene mScene;
};

int main() {
    kitgui::Context::init(kitgui::WindowApi::Any);

    {
        kitgui::Context ctx1([](kitgui::Context& ctx) { return std::make_unique<MyApp>(ctx); });
        ctx1.Create(true);
        ctx1.Show();

        kitgui::Context::RunLoop();
    }

    kitgui::Context::deinit();
}