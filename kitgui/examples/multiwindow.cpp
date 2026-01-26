#include <imgui.h>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"

using namespace Magnum;

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
    kitgui::Context::init(kitgui::WindowApi::Any, "kitgui");

    {
        kitgui::Context ctx1([](kitgui::Context& ctx) { return std::make_unique<MyApp2>(ctx); });
        ctx1.Create(true);

        kitgui::Context ctx2([](kitgui::Context& ctx) { return std::make_unique<MyApp2>(ctx); });
        ctx2.Create(true);

        ctx1.Show();
        ctx2.Show();

        kitgui::Context::RunLoop();
    }

    kitgui::Context::deinit();
}