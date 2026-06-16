#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <imgui.h>
#include "kitgui/types.h"

namespace kitgui {
class Context;

/**
 * Represents a loaded texture, as well as the basics to display it on screen.
 */
class Image {
   public:
    explicit Image(kitgui::Context& mContext);
    ~Image();
    void Load(std::string_view path);
    ImTextureID GetId();

   private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
}  // namespace kitgui
