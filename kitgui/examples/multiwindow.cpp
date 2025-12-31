#include <Magnum/Math/Color.h>
#include <imgui.h>
#include <memory>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/domKnob.h"
#include "kitgui/domScene.h"
#include "kitgui/kitgui.h"

using namespace Magnum;

class MyApp : public kitgui::BaseApp {
   public:
    explicit MyApp(kitgui::Context& mContext) : kitgui::BaseApp(mContext), mScene(kitgui::DomScene::Create(mContext)) {
        mContext.SetSizeConfig({400, 400});
        mContext.SetClearColor({0.3f, 0.7f, 0.3f, 1.0f});

        auto props = mScene->GetProps();
        props.scenePath = "assets/duck.glb";
        mScene->SetProps(props);

        auto knob = kitgui::DomKnob::Create();
        mScene->Insert(knob.get(), nullptr);
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

class MyApp2 : public kitgui::BaseApp {
   public:
    explicit MyApp2(kitgui::Context& mContext) : kitgui::BaseApp(mContext) {}
    ~MyApp2() = default;

   protected:
    void OnActivate() override {}
    void OnUpdate() override { ImGui::Text("Window.... 2!!!"); }

    void OnDraw() override {}

   private:
};

int main() {
    kitgui::Context::init();

    {
        kitgui::Context ctx1([](kitgui::Context& ctx) { return std::make_unique<MyApp2>(ctx); });
        ctx1.Create(kitgui::WindowApi::Any, true);

        kitgui::Context ctx2([](kitgui::Context& ctx) { return std::make_unique<MyApp2>(ctx); });
        ctx2.Create(kitgui::WindowApi::Any, true);

        ctx1.Show();
        ctx2.Show();

        kitgui::Context::RunLoop();
    }

    kitgui::Context::deinit();
}