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
    void Add(const MonoAudioBuffer& in) {
        for(size_t i = 0; i < data.size(); ++i)
        {
            data[i] += in.data[i];
        }
        isConstant = isConstant && in.isConstant;
    }
};

class MonoAudioScratchBuffer {
    explicit MonoAudioScratchBuffer(size_t maxBlockSize = 0) : mMemory(maxBlockSize, 0.0f) {}

    void Resize(size_t maxBlockSize) {
        mMemory.assign(maxBlockSize, 0.0f);
    }

    MonoAudioBuffer Alloc(size_t blockSize) {
        return {etl::span<float>(mMemory.data(), blockSize), false};
    }

    MonoAudioBuffer Alloc(const MonoAudioBuffer& existing) {
        std::copy(existing.data.begin(), existing.data.end(), mMemory.begin());
        return {etl::span<float>(mMemory.data(), existing.data.size()), existing.isConstant};
    }

    private:
    std::vector<float> mMemory;
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
    void Add(const StereoAudioBuffer& in) {
        for(size_t i = 0; i < left.size(); ++i)
        {
            left[i] += in.left[i];
        }
        for(size_t i = 0; i < right.size(); ++i)
        {
            right[i] += in.right[i];
        }
        isLeftConstant = isLeftConstant && in.isLeftConstant;
        isRightConstant = isRightConstant && in.isRightConstant;
    }
};

class StereoAudioScratchBuffer {
    public:
    explicit StereoAudioScratchBuffer(size_t maxBlockSize = 0)
        : mLeftMemory(maxBlockSize, 0.0f), mRightMemory(maxBlockSize, 0.0f) {}

    void Resize(size_t maxBlockSize) {
        mLeftMemory.assign(maxBlockSize, 0.0f);
        mRightMemory.assign(maxBlockSize, 0.0f);
    }

    StereoAudioBuffer Alloc(size_t blockSize) {
        std::fill(mLeftMemory.begin(), mLeftMemory.begin()+blockSize, 0.0f);
        std::fill(mRightMemory.begin(), mRightMemory.begin()+blockSize, 0.0f);
        return {
            etl::span<float>(mLeftMemory.data(), blockSize),
            etl::span<float>(mRightMemory.data(), blockSize),
            false,
            false
        };
    }

    StereoAudioBuffer Alloc(const StereoAudioBuffer& existing) {
        std::copy(existing.left.begin(), existing.left.end(), mLeftMemory.begin());
        std::copy(existing.right.begin(), existing.right.end(), mRightMemory.begin());
        return {
            etl::span<float>(mLeftMemory.data(), existing.left.size()),
            etl::span<float>(mRightMemory.data(), existing.right.size()),
            existing.isLeftConstant,
            existing.isRightConstant
        };
    }

    private:
    std::vector<float> mLeftMemory;
    std::vector<float> mRightMemory;
};

// All of the identifying info for a given note.
// Sorted from most specific to least specific: id trumps port-channel-key and so on
// -1 means wildcard/no value, >= 0 is a specific value.
struct NoteTuple {
    static constexpr int16_t kNone = -1;
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
    auto operator<=>(const NoteTuple&) const = default;
};

}  // namespace clapeze