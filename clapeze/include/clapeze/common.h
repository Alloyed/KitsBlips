#pragma once

#include <etl/span.h>
#include <cstdint>

namespace clapeze {

struct MonoAudioBuffer {
    etl::span<float> data;
    bool isConstant;
    void CopyFrom(const MonoAudioBuffer& in) {
        isConstant = in.isConstant;
        std::copy(in.data.begin(), in.data.end(), data.begin());
    }
    void Fill(float value) {
        std::fill(data.begin(), data.end(), value);
        isConstant = true;
    }
};

struct StereoAudioBuffer {
    etl::span<float> left;
    etl::span<float> right;
    bool isLeftConstant;
    bool isRightConstant;
    void CopyFrom(const MonoAudioBuffer& in) {
        isLeftConstant = in.isConstant;
        std::copy(in.data.begin(), in.data.end(), left.begin());
        isRightConstant = in.isConstant;
        std::copy(in.data.begin(), in.data.end(), right.begin());
    }
    void CopyFrom(const StereoAudioBuffer& in) {
        isLeftConstant = in.isLeftConstant;
        std::copy(in.left.begin(), in.left.end(), left.begin());
        isRightConstant = in.isRightConstant;
        std::copy(in.right.begin(), in.right.end(), right.begin());
    }
    void Fill(float value) {
        std::fill(left.begin(), left.end(), value);
        std::fill(right.begin(), right.end(), value);
        isLeftConstant = true;
        isRightConstant = true;
    }
};

// All of the identifying info for a given note.
// -1 means wildcard/no value, >= 0 is a specific value.
struct NoteTuple {
    static constexpr int8_t kNone = -1;
    int32_t id = kNone;
    int16_t port = kNone;
    int16_t channel = kNone;
    int16_t key = kNone;
    bool Match(const NoteTuple& other) const {
        if (id != -1 && other.id != -1) {
            // if ids match, or don't, no need to check everything else
            return id == other.id;
        }
        if (port != -1 && other.port != -1 && port != other.port) {
            return false;
        }
        if (channel != -1 && other.channel != -1 && channel != other.channel) {
            return false;
        }
        if (key != -1 && other.key != -1 && key != other.key) {
            return false;
        }
        return true;
    }
};

}  // namespace clapeze
