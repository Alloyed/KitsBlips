#include "kitgui/context.h"
#include "kitgui/kitgui.h"

int main() {
    kitgui::init();

    kitgui::Context ctx1{};
    ctx1.Create(kitgui::platform::Api::Any, true);

    kitgui::Context ctx2{};
    ctx2.Create(kitgui::platform::Api::Any, true);

    ctx1.Show();
    ctx2.Show();

    // update loop here
    ctx1.Destroy();
    ctx2.Destroy();
    kitgui::deinit();
}