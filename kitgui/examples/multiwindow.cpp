#include <memory>
#include "imgui.h"
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"

class MyApp : public kitgui::BaseApp {
   public:
    MyApp(kitgui::Context& mContext) : kitgui::BaseApp(mContext) {}
    ~MyApp() = default;

   protected:
    void OnUpdate() override {
        ImGuiHelpers::beginMain([&]() {
            ImGui::Text("Oh yeah, gamer time!");
            // double inout = params.GetRaw(Params::Rise);
            // if (kitgui::knob("Rise", static_cast<const NumericParam&>(*params.GetConfig(Params::Rise)), {}, inout)) {
            //     params.SetRaw(Params::Rise, inout);
            // }
        });
    }
};

int main() {
    kitgui::init();

    kitgui::Context ctx1{};
    ctx1.SetApp(std::make_shared<MyApp>(ctx1));
    ctx1.Create(kitgui::platform::Api::Any, true);

    kitgui::Context ctx2{};
    ctx2.SetApp(std::make_shared<MyApp>(ctx2));
    ctx2.Create(kitgui::platform::Api::Any, true);

    ctx1.Show();
    ctx2.Show();

    kitgui::Context::RunLoop();

    // update loop here
    kitgui::deinit();
}