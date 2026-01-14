#include <imgui.h>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"

class MyApp : public kitgui::BaseApp {
   public:
    explicit MyApp(kitgui::Context& mContext) : kitgui::BaseApp(mContext) {}
    ~MyApp() = default;

   protected:
    void OnActivate() override {}
    void OnUpdate() override {
        ImGui::Text("base window");
        ImGui::ShowDemoWindow();
    }

    void OnDraw() override {}

   private:
};

int main() {
    kitgui::Context::init(kitgui::WindowApi::Any, "kitgui");

    {
        kitgui::Context ctx1([](kitgui::Context& ctx) { return std::make_unique<MyApp>(ctx); });
        ctx1.Create(true);
        ctx1.Show();

        kitgui::Context::RunLoop();
    }

    kitgui::Context::deinit();
}