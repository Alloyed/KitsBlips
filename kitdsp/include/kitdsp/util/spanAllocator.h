#pragma once

#include <etl/span.h>
#include <vector>

namespace kitdsp {

/**
 * Implements a "bump" allocator backed by a fixed span of externally provided memory.
 */
template <typename T>
class SubSpanAllocator {
   public:
    explicit SubSpanAllocator(etl::span<T> mem) : mMemory(mem) {}

    /**
     * allocates a new block of memory and returns it as a span<T>. The rules for ownership of this memory are the same
     * as that of the `mem` block you passed in the constructor.
     */
    etl::span<T> alloc(size_t size) {
        size_t idx = mNextIdx;
        mNextIdx += size;
        assert(idx + size <= mMemory.size());
        return mMemory.subspan(idx, size);
    }

    /**
     * Resets the internal bump pointer of the allocator. This means that any memory previously allocated should be
     * considered "invalid", and that space will get re-used.
     */
    void reset() { mNextIdx = 0; }

   private:
    etl::span<T> mMemory;
    size_t mNextIdx = 0;
};

/**
 * Implements a direct allocator: each call to "alloc" will perform a call to new[] under the hood.
 */
template <typename T>
class DynamicSpanAllocator {
   public:
    explicit DynamicSpanAllocator() {}
    DynamicSpanAllocator(const DynamicSpanAllocator&) = delete;
    DynamicSpanAllocator(DynamicSpanAllocator&&) = delete;
    DynamicSpanAllocator& operator=(const DynamicSpanAllocator&) = delete;
    DynamicSpanAllocator& operator=(DynamicSpanAllocator&&) = delete;
    ~DynamicSpanAllocator() { reset(); }

    /**
     * allocates a new block of memory as a span<T>. The allocator itself owns this memory: it remains valid until
     * reset() is called or the allocator is destructed.
     */
    etl::span<T> alloc(size_t size) {
        mMemory.push_back(new T[size]);
        return {mMemory.back(), size};
    }

    /**
     * calls delete[] on all previously allocated blocks of memory. doing this invalidates them!
     */
    void reset() {
        for (T* block : mMemory) {
            delete[] block;
        }
    }

   private:
    std::vector<T*> mMemory;
};

}  // namespace kitdsp