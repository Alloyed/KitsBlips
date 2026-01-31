#include <imgui.h>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"

using namespace Magnum;

class MyApp2 : public kitgui::BaseApp {
   public:
    explicit MyApp2(kitgui::Context& mContext, int32_t idx) : kitgui::BaseApp(mContext), mIdx(idx) {}
    ~MyApp2() = default;

   protected:
    void OnActivate() override {}
    void OnUpdate() override {
        ImGui::Text("Window: %d", mIdx);
        ImGui::Text("Scale: %f", GetContext().GetUIScale());
    }

    void OnDraw() override {}

   private:
    int32_t mIdx;
};

int main() {
    kitgui::Context::init(kitgui::WindowApi::Any, "kitgui");

    {
        kitgui::Context ctx1([](kitgui::Context& ctx) { return std::make_unique<MyApp2>(ctx, 1); });
        ctx1.SetClearColor({0.4f, 0.1f, 0.4f, 1.0f});
        ctx1.Create(true);

        kitgui::Context ctx2([](kitgui::Context& ctx) { return std::make_unique<MyApp2>(ctx, 2); });
        ctx2.SetClearColor({0.1f, 0.4f, 0.4f, 1.0f});
        ctx2.Create(true);

        ctx1.Show();
        ctx2.Show();

        kitgui::Context::RunLoop();
    }

    kitgui::Context::deinit();
}