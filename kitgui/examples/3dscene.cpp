#include <Magnum/Math/Color.h>
#include <imgui.h>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/dom/domScene.h"
#include "kitgui/kitgui.h"

class MyApp : public kitgui::BaseApp {
   public:
    explicit MyApp(kitgui::Context& mContext) : kitgui::BaseApp(mContext), mScene(kitgui::DomScene::Create(mContext)) {
        mContext.SetSizeConfig({400, 400});
        mContext.SetClearColor(Magnum::Math::Color4(0.3f, 0.7f, 0.3f, 1.0f));

        auto props = mScene->GetProps();
        props.scenePath = "../assets/duck.glb";
        mScene->SetProps(props);
    }
    ~MyApp() = default;

   protected:
    void OnActivate() override { mScene->Load(); }
    void OnUpdate() override {
        mScene->Visit([](kitgui::DomNode& node) {
            node.Update();
            return true;
        });
        ImGui::Text("Oh yeah, gamer time!");
    }

    void OnDraw() override {
        mScene->Visit([](kitgui::DomNode& node) {
            node.Draw();
            return true;
        });
    }

   private:
    double mKnob = 0.0;
    std::shared_ptr<kitgui::DomScene> mScene;
};

int main() {
    kitgui::Context::init();

    {
        kitgui::Context ctx1([](kitgui::Context& ctx) { return std::make_unique<MyApp>(ctx); });
        ctx1.Create(kitgui::WindowApi::Any, true);
        ctx1.Show();

        kitgui::Context::RunLoop();
    }

    kitgui::Context::deinit();
}