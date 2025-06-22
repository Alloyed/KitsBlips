#include "kitgui/context.h"
#include "kitgui/kitgui.h"

int main() {
    kitgui::init();

    kitgui::Context ctx1(kitgui::platform::Api::Any);
    ctx1.Init();

    kitgui::Context ctx2(kitgui::platform::Api::Any);
    ctx2.Init();

    ctx1.Show();
    ctx2.Show();

    // update loop here

    kitgui::deinit();
}