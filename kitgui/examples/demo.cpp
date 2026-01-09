#include <Magnum/Math/Color.h>
#include <imgui.h>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/gfx/scene.h"
#include "kitgui/kitgui.h"

class MyApp : public kitgui::BaseApp {
   public:
    explicit MyApp(kitgui::Context& mContext) : kitgui::BaseApp(mContext) {
        mContext.SetSizeConfig({800, 800});
        mContext.SetClearColor(Magnum::Math::Color4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    ~MyApp() = default;

   protected:
    void OnUpdate() override { ImGui::ShowDemoWindow(); }

   private:
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