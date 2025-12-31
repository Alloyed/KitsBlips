#pragma once

#include "kitgui/domNode.h"
#include <memory>
#include <string>

namespace kitgui {
class Context;

struct DomSceneImpl;
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
    DomScene(kitgui::Context& mContext);
    Props mProps{};
    std::unique_ptr<DomSceneImpl> mImpl;
};

}  // namespace kitgui

