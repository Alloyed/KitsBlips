#pragma once

#include <Magnum/Animation/Player.h>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace Magnum {
namespace Trade {
class AbstractImporter;
}
}  // namespace Magnum

namespace kitgui {
struct ObjectInfo;
using AnimationPlayer = Magnum::Animation::Player<std::chrono::nanoseconds, float>;
struct AnimationInfo {
    AnimationPlayer player;
    std::string name;
};

struct AnimationCache {
    std::vector<AnimationInfo> mAnimations;
    void LoadAnimations(Magnum::Trade::AbstractImporter& importer, std::vector<ObjectInfo>& mSceneObjects);
};
}  // namespace kitgui