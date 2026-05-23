#pragma once

#include <etl/span.h>
#include <vector>

namespace kitdsp {
template <typename T>
class SpanAllocator {
   public:
    explicit SpanAllocator(etl::span<T> mem) : mMemory(mem) {}

    etl::span<T> alloc(size_t size) {
        size_t idx = mNextIdx;
        mNextIdx += size;
        assert(idx + size <= mMemory.size());
        return mMemory.subspan(idx, size);
    }

   private:
    etl::span<T> mMemory;
    size_t mNextIdx = 0;
};

template <typename T>
class DynamicSpanAllocator {
   public:
    explicit DynamicSpanAllocator() {}

    etl::span<T> alloc(size_t size) {
        mMemory.push_back(new T[size]);
        return {mMemory.back(), size};
    }

    void reset() {
        for (T* block : mMemory) {
            delete[] block;
        }
    }

   private:
    std::vector<T*> mMemory;
};

}  // namespace kitdsp