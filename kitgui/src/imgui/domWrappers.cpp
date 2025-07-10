#include "kitgui/dom.h"

#include "imgui/knob.h"

namespace kitgui {
std::shared_ptr<DomKnob> DomKnob::Create() {
    return std::shared_ptr<DomKnob>(new DomKnob());
}
DomKnob::DomKnob() = default;
DomKnob::~DomKnob() = default;
void DomKnob::Update() {
    double rawInOut;
    KnobConfig cfg;
    kitgui::knob("id", cfg, rawInOut);
}

const DomKnob::Props& DomKnob::GetProps() const {
    return mProps;
}

void DomKnob::SetProps(const DomKnob::Props& props) {
    // TODO: dirty flagging
    mProps = props;
}

}  // namespace kitgui