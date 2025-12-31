#pragma once

#include "kitgui/domNode.h"
#include <Magnum/Math/Vector2.h>
#include <memory>
#include <optional>
#include <string>

namespace kitgui {

class DomKnob : public DomNode {
   public:
    struct Props {
        std::optional<Magnum::Vector2> position;
        std::string id{};
        std::string sceneAnchor{};
    };
    static std::shared_ptr<DomKnob> Create();
    ~DomKnob() override;
    const Props& GetProps() const;
    void SetProps(const Props& props);
    void Update() override;
    void Draw() override {};

   private:
    DomKnob();
    Props mProps{};
};

}  // namespace kitgui

