#include "kitgui/dom.h"

#include <imgui.h>
#include "imguiHelpers/knob.h"

namespace kitgui {
std::shared_ptr<DomKnob> DomKnob::Create() {
    return std::shared_ptr<DomKnob>(new DomKnob());
}
DomKnob::DomKnob() = default;
DomKnob::~DomKnob() = default;
void DomKnob::Update() {
    ImVec2 oldPos = ImGui::GetCursorScreenPos();

    if (mProps.position) {
        ImGui::SetCursorScreenPos({mProps.position->x(), mProps.position->y()});
    }
    double rawInOut;
    ImGuiHelpers::KnobConfig cfg;
    ImGuiHelpers::knob(mProps.id.c_str(), cfg, rawInOut);

    ImGui::SetCursorScreenPos(oldPos);
}

const DomKnob::Props& DomKnob::GetProps() const {
    return mProps;
}

void DomKnob::SetProps(const DomKnob::Props& props) {
    // TODO: dirty flagging
    mProps = props;
}

}  // namespace kitgui