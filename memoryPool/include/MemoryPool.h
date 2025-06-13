#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include "Common.h"
#include "ThreadCache.h"

namespace memory_pool
{

    class MemoryPool
    {
    public:
        static void *allocate(size_t size)
        {
            return ThreadCache::Instance()->allocate(size);
        }

        static void deallocate(void *ptr, size_t size)
        {
            return ThreadCache::Instance()->deallocate(ptr, size);
        }
    };

    // ÄÚ´æ³ØÔ¤ÈÈ
    static void memoryPoolWarmUp()
    {
        constexpr int loopNums = 10000;
        constexpr size_t allocSizes[] = {8, 16, 32, 64, 128, 256, 512, 1024};

        std::vector<std::pair<void *, size_t>> ptrVec;
        ptrVec.reserve(loopNums * std::size(allocSizes));

        for (int i = 0; i < loopNums; ++i)
        {
            for (size_t size : allocSizes)
            {
                void *p = MemoryPool::allocate(size);
                assert(p != nullptr);
                ptrVec.emplace_back(p, size);
            }
        }

        for (auto &[p, s] : ptrVec)
            MemoryPool::deallocate(p, s);
    }

}

#endif // MEMORY_POOL_H