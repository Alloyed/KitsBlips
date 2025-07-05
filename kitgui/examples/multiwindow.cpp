#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <imgui.h>
#include <memory>
#include "gfx/gltfScene.h"
#include "imgui/imguiHelpers.h"
#include "imgui/knob.h"
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"

using namespace Magnum;

class MyApp2 : public kitgui::BaseApp {
   public:
    MyApp2(kitgui::Context& mContext) : kitgui::BaseApp(mContext) {}
    ~MyApp2() = default;

   protected:
    void OnActivate() override {}
    void OnUpdate() override {
        kitgui::helpers::beginFullscreen([&]() { ImGui::Text("Window.... 2!!!"); });
    }

    void OnDraw() override {}

   private:
    double mKnob = 0.0;
};

class MyApp : public kitgui::BaseApp {
   public:
    MyApp(kitgui::Context& mContext, Magnum::Trade::AbstractImporter& importer)
        : kitgui::BaseApp(mContext), mImporter(importer) {
        mContext.SetSize(400, 400);
        mContext.SetClearColor({0.3f, 0.7f, 0.3f, 1.0f});
    }
    ~MyApp() = default;

   protected:
    void OnActivate() override {
        GL::Renderer::enable(GL::Renderer::Feature::DebugOutput);
        GL::Renderer::enable(GL::Renderer::Feature::DebugOutputSynchronous);
        GL::DebugOutput::setDefaultCallback();
        mScene.Load(mImporter, "thingy.glb");
    }
    void OnUpdate() override {
        mScene.Update();
        kitgui::helpers::beginFullscreen([&]() {
            ImGui::Text("Oh yeah, gamer time!");
            kitgui::knob("Rise", {}, mKnob);
        });
    }

    void OnDraw() override { mScene.Draw(); }

   private:
    double mKnob = 0.0;
    kitgui::GltfScene mScene;
    Magnum::Trade::AbstractImporter& mImporter;
};

int main() {
    CORRADE_RESOURCE_INITIALIZE(embeddedAssets);
    Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter> manager;
    Corrade::Containers::Pointer<Magnum::Trade::AbstractImporter> importer = manager.loadAndInstantiate("GltfImporter");
    Magnum::Trade::AbstractImporter* raw = importer.get();
    Utility::Resource rs{"embeddedAssets"};
    Containers::ArrayView<const char> data = rs.getRaw("duck.glb");
    importer->openData(data);

    kitgui::init();

    {
        kitgui::Context ctx1([raw](kitgui::Context& ctx) { return std::make_unique<MyApp2>(ctx); });
        ctx1.Create(kitgui::platform::Api::Any, true);

        kitgui::Context ctx2([raw](kitgui::Context& ctx) { return std::make_unique<MyApp2>(ctx); });
        ctx2.Create(kitgui::platform::Api::Any, true);

        ctx1.Show();
        ctx2.Show();

        kitgui::Context::RunLoop();
    }

    kitgui::deinit();
}