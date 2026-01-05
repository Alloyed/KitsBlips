#pragma once

#include <memory>
#include <string>
#include "kitgui/dom/domNode.h"

namespace kitgui {
class Context;

struct Scene;
class DomScene : public DomNode {
   public:
    struct Props {
        std::string scenePath{};
    };
    static std::shared_ptr<DomScene> Create(kitgui::Context& mContext);
    ~DomScene() override;

    const Props& GetProps() const;
    void SetProps(const Props& props);
    void Load();
    void Update() override;
    void Draw() override;

   private:
    explicit DomScene(kitgui::Context& mContext);
    Props mProps{};
    std::unique_ptr<Scene> mImpl;
};

}  // namespace kitgui
