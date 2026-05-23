#include <imgui.h>
#include <nfd.h>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"
#include "kitgui/wrap_nfd.h"

class MyApp : public kitgui::BaseApp {
   public:
    explicit MyApp(kitgui::Context& mContext) : kitgui::BaseApp(mContext) {}
    ~MyApp() = default;

   protected:
    void OnActivate() override {}
    void OnUpdate() override {
        static std::string resultstr = "";

        if (ImGui::Button("Open file")) {
            nfdu8char_t* outPath{};
            nfdu8filteritem_t filters[2] = {{"Source code", "c,cpp,cc"}, {"Headers", "h,hpp"}};
            nfdopendialogu8args_t args = {0};
            args.filterList = filters;
            args.filterCount = 2;
            args.parentWindow = NFD_GetWindow(GetContext().GetWindow());
            nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);
            if (result == NFD_OKAY) {
                resultstr = outPath;
                NFD_FreePathU8(outPath);
            } else if (result == NFD_CANCEL) {
                resultstr = "<canceled>";
            } else {
                resultstr = NFD_GetError();
            }
        }
        ImGui::Text("%s", resultstr.c_str());
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