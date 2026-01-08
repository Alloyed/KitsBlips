// IWYU pragma: begin_keep
#include <Corrade/Containers/OptionalStl.h>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/StringStl.h>
// IWYU pragma: end_keep

#include <Magnum/Math/CubicHermite.h>
#include <Magnum/Trade/AnimationData.h>
#include <cassert>
#include "Magnum/Trade/AbstractImporter.h"
#include "fmt/format.h"
#include "gfx/animations.h"
#include "gfx/sceneGraph.h"
#include "log.h"

using namespace Magnum;

namespace kitgui {
void AnimationCache::LoadAnimations(Magnum::Trade::AbstractImporter& importer, std::vector<ObjectInfo>& sceneObjects) {
    /* Load all animations. Animations that fail to load will be NullOpt. */
    kitgui::log::info(fmt::format("Loading {} animations...", importer.animationCount()));
    mAnimations.clear();
    mAnimations.reserve(importer.animationCount());
    for (uint32_t i = 0; i != importer.animationCount(); ++i) {
        kitgui::log::info(fmt::format("loading: {}/animation:{}:{}", "", i, importer.animationName(i)));
        auto animation = importer.animation(i);
        if (!animation) {
            kitgui::log::error(fmt::format("cannot load: {}/animation:{}:{}", "", i, importer.animationName(i)));
            continue;
        }
        mAnimations.push_back(
            AnimationInfo{Magnum::Animation::Player<std::chrono::nanoseconds, float>(), importer.animationName(i)});
        auto& animationPlayer = mAnimations.back().player;

        for (uint32_t j = 0; j != animation->trackCount(); ++j) {
            if (animation->trackTarget(j) >= sceneObjects.size() || !sceneObjects[animation->trackTarget(j)].object) {
                continue;
            }

            Object3D& animatedObject = *sceneObjects[animation->trackTarget(j)].object;

            switch (animation->trackTargetName(j)) {
                case Trade::AnimationTrackTarget::Translation3D: {
                    const auto callback = [](float, const Vector3& translation, Object3D& object) {
                        object.setTranslation(translation);
                    };
                    if (animation->trackType(j) == Trade::AnimationTrackType::CubicHermite3D) {
                        animationPlayer.addWithCallback(animation->track<CubicHermite3D>(j), callback, animatedObject);
                    } else {
                        assert(animation->trackType(j) == Trade::AnimationTrackType::Vector3);
                        animationPlayer.addWithCallback(animation->track<Vector3>(j), callback, animatedObject);
                    }
                    break;
                }
                case Trade::AnimationTrackTarget::Rotation3D: {
                    const auto callback = [](float, const Quaternion& rotation, Object3D& object) {
                        object.setRotation(rotation);
                    };
                    if (animation->trackType(j) == Trade::AnimationTrackType::CubicHermiteQuaternion) {
                        animationPlayer.addWithCallback(animation->track<CubicHermiteQuaternion>(j), callback,
                                                        animatedObject);
                    } else {
                        assert(animation->trackType(j) == Trade::AnimationTrackType::Quaternion);
                        animationPlayer.addWithCallback(animation->track<Quaternion>(j), callback, animatedObject);
                    }
                    break;
                }
                case Trade::AnimationTrackTarget::Scaling3D: {
                    const auto callback = [](float, const Vector3& scaling, Object3D& object) {
                        object.setScaling(scaling);
                    };
                    if (animation->trackType(j) == Trade::AnimationTrackType::CubicHermite3D) {
                        animationPlayer.addWithCallback(animation->track<CubicHermite3D>(j), callback, animatedObject);
                    } else {
                        assert(animation->trackType(j) == Trade::AnimationTrackType::Vector3);
                        animationPlayer.addWithCallback(animation->track<Vector3>(j), callback, animatedObject);
                    }
                    break;
                }
                default: {
                    break;
                }
            }
        }
    }
}
}  // namespace kitgui