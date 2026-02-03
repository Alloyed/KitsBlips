#pragma once

#include <optional>
#include <string>
#include "fmt/format.h"
#include "kitgui/types.h"

namespace kitgui {
class ToggleSwitch {
   public:
    virtual ~ToggleSwitch() = default;
    bool Update(bool& value);

    // defaults to showing for imgui display
    bool mShowDebug = true;
    // if not provided, uses imgui cursor. represents top left
    std::optional<Vector2> mPos = {};
    // if not provided, works off of imgui normal sizing
    std::optional<float> mWidth = {};

   protected:
    virtual const std::string& GetName() const {
        static std::string s = "ToggleSwitch";
        return s;
    }
    virtual bool GetDefault() const { return false; }
};
}  // namespace kitgui
