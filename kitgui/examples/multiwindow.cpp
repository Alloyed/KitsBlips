#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/Resource.h>
#include <imgui.h>
#include <memory>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/dom.h"
#include "kitgui/kitgui.h"

using namespace Magnum;

class MyApp2 : public kitgui::BaseApp {
   public:
    MyApp2(kitgui::Context& mContext) : kitgui::BaseApp(mContext) {}
    ~MyApp2() = default;

   protected:
    void OnActivate() override {}
    void OnUpdate() override { ImGui::Text("Window.... 2!!!"); }

    void OnDraw() override {}

   private:
};

class MyApp : public kitgui::BaseApp {
   public:
    MyApp(kitgui::Context& mContext) : kitgui::BaseApp(mContext), mScene(kitgui::DomScene::Create()) {
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

int main() {
    // CORRADE_RESOURCE_INITIALIZE(embeddedAssets);
    // Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter> manager;
    // Corrade::Containers::Pointer<Magnum::Trade::AbstractImporter> importer =
    // manager.loadAndInstantiate("GltfImporter"); Magnum::Trade::AbstractImporter* raw = importer.get();
    // Utility::Resource rs{"embeddedAssets"};
    // Containers::ArrayView<const char> data = rs.getRaw("duck.glb");
    // importer->openData(data);

    kitgui::init();

    {
        kitgui::Context ctx1([](kitgui::Context& ctx) { return std::make_unique<MyApp2>(ctx); });
        ctx1.Create(kitgui::platform::Api::Any, true);

        kitgui::Context ctx2([](kitgui::Context& ctx) { return std::make_unique<MyApp2>(ctx); });
        ctx2.Create(kitgui::platform::Api::Any, true);

        ctx1.Show();
        ctx2.Show();

        kitgui::Context::RunLoop();
    }

    kitgui::deinit();
}