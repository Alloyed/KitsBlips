#pragma once

#include <optional>
#include <string>
#include "fmt/format.h"
#include "kitgui/types.h"

namespace kitgui {
class Knob {
   public:
    virtual ~Knob() = default;
    bool Update(double& value);

    // defaults to showing for imgui display
    bool mShowDebug = true;
    // if not provided, uses imgui cursor
    std::optional<Vector2> mPos = {};
    // if not provided, works off of imgui normal sizing
    std::optional<float> mWidth = {};

    float minAngleRadians = kPi * 0.75f;
    float maxAngleRadians = kPi * 2.25f;

   protected:
    virtual const std::string& GetName() const {
        static std::string s = "Knob";
        return s;
    }
    virtual double GetDefault() const { return 0.5; }
    virtual std::string FormatValueText(double value) const { return fmt::format("{}", value); }

   private:
    static constexpr float kPi = 3.14159265359;
};
}  // namespace kitgui