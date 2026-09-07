#pragma once

#include <cmath>
#include "kitdsp/math/hash.h"
#include "kitdsp/math/util.h"

namespace kitdsp {
    class WhiteNoise {
        public:
            // [0, 1]
            float ProcessNormalized() {
                counter++;
                return hash(counter);
            }
            // [-1, 1]
            float Process() {
                return ProcessNormalized() * 2.0f - 1.0f;
            }
            // [min, max]
            int32_t ProcessInt(int32_t min, int32_t max) {
                int32_t diff = max - min;
                return clamp(narrow_cast<int32_t>(ProcessNormalized() * diff), min, max) + min;
            }
            // [0, length-1]
            size_t ProcessIndex(size_t length) {
                return narrow_cast<size_t>(ProcessInt(0, narrow_cast<int32_t>(length)-1));
            }
            void Reset() {
                counter = 0;
            }
        private:
            uint32_t counter = 0;
    };
}  // namespace kitdsp
