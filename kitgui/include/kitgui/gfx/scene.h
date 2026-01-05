#pragma once

#include <memory>
#include <string_view>

namespace kitgui {
class Context;
class Scene {
   public:
    explicit Scene(kitgui::Context& mContext);
    ~Scene();
    void Load(std::string_view path);
    void Update();
    void Draw();

   private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
}  // namespace kitgui
