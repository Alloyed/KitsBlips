#pragma once

#include <cmath>
#include "kitdsp/math/hash.h"
#include "kitdsp/math/util.h"

namespace kitdsp {
    class WhiteNoise {
        public:
            // [-1, 1]
            float Process() {
                counter++;
                return hash(counter) * 2.0f - 1.0f;
            }
            // [0, 1]
            float ProcessNormalized() {
                counter++;
                return hash(counter);
            }
            // [min, max]
            int32_t ProcessInt(int32_t min, int32_t max) {
                counter++;
                int32_t diff = max - min;
                return clamp(narrow_cast<int32_t>(hash(counter) * diff), min, max) + min;
            }
            void Reset() {
                counter = 0;
            }
        private:
            uint32_t counter = 0;
    };
}  // namespace kitdsp
