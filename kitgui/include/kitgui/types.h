#pragma once

#include "Magnum/Math/Color.h"    // IWYU pragma: export
#include "Magnum/Math/Vector2.h"  // IWYU pragma: export

namespace kitgui {
// All magnum types that exist in the public interface should go through here instead
using Vector2 = Magnum::Vector2;
using Color4 = Magnum::Color4;
}  // namespace kitgui