#pragma once

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace clapeze {
/**
 * dynamically sized bitset, optimized for holding lots of dirty flags.
 */
class DirtySet {
   public:
    explicit DirtySet(size_t numBits) : mNumBits(numBits), mWords(GetNumWords(), 0) {}

    void Reset() { std::fill(mWords.begin(), mWords.end(), 0); }
    void Set(size_t idx, bool value = true) {
        size_t wordIdx{};
        uint64_t mask{};
        Split(idx, wordIdx, mask);
        if (value) {
            mWords[wordIdx] |= mask;
        } else {
            mWords[wordIdx] &= ~mask;
        }
    }
    void Unset(size_t idx) { Set(idx, false); }
    bool Test(size_t idx) {
        size_t wordIdx{};
        uint64_t mask{};
        Split(idx, wordIdx, mask);
        return (mWords[wordIdx] & mask) != 0;
    }

    size_t Count() {
        size_t sum = 0;
        for (const auto& word : mWords) {
            sum += std::popcount(word);
        }
        return sum;
    }
    bool Any() {
        for (const auto& word : mWords) {
            if (word > 0) {
                return true;
            }
        }
        return false;
    }
    template <size_t... Idxs>
    bool AnyOf() const {
        std::vector<uint64_t> masks(GetNumWords(), 0);
        ((masks[Idxs / 64] |= (1 << (Idxs % 64))), ...);

        for (size_t i = 0; i < masks.size(); ++i) {
            if ((mWords[i] & masks[i]) != 0) {
                return true;
            }
        }

        return false;
    }

   private:
    static void Split(size_t idx, size_t& wordIdx, uint64_t& mask) {
        wordIdx = idx / 64;
        mask = 1 << (idx % 64);
    }
    size_t GetNumWords() const { return static_cast<size_t>(std::ceil(static_cast<double>(mNumBits) / 64.0)); }
    size_t mNumBits;
    std::vector<uint64_t> mWords;
};
}  // namespace clapeze