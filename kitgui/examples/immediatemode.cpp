#include <imgui.h>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/controls/knob.h"
#include "kitgui/kitgui.h"

class MyApp : public kitgui::BaseApp {
   public:
    explicit MyApp(kitgui::Context& mContext) : kitgui::BaseApp(mContext) {}
    ~MyApp() = default;

   protected:
    void OnActivate() override {}
    void OnUpdate() override {
        static kitgui::Knob knob1;
        static kitgui::Knob knob2;
        static kitgui::Knob knob3;

        knob1.Update(mValue);
        knob2.Update(mValue);
        knob3.Update(mValue);
    }

    void OnDraw() override {}

   private:
    double mValue = 0.5;
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