#pragma once

#include <etl/span.h>
#include <cstdint>

struct MonoAudioBuffer {
    etl::span<float> data;
    bool isConstant;
};

struct StereoAudioBuffer {
    etl::span<float> left;
    etl::span<float> right;
    bool isLeftConstant;
    bool isRightConstant;
};

// All of the identifying info for a given note.
// -1 means wildcard/no value, >= 0 is a specific value.
struct NoteTuple {
    int32_t id = -1;
    int16_t port = -1;
    int16_t channel = -1;
    int16_t key = -1;
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
