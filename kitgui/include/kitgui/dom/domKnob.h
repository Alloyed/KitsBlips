#pragma once

#include <memory>
#include <optional>
#include <string>
#include "kitgui/dom/domNode.h"
#include "kitgui/types.h"

namespace kitgui {

class DomKnob : public DomNode {
   public:
    struct Props {
        std::optional<Vector2> position;
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
