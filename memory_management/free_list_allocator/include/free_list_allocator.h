#ifndef MEMORY_MANAGEMENT_FREE_LIST_ALLOCATOR_INCLUDE_FREE_LIST_ALLOCATOR_H
#define MEMORY_MANAGEMENT_FREE_LIST_ALLOCATOR_INCLUDE_FREE_LIST_ALLOCATOR_H

#include <cstddef>
#include <new>
#include <sys/mman.h>
#include <unistd.h>
#include "base/macros.h"

template <size_t ONE_MEM_POOL_SIZE>
class FreeListAllocator {
    // here we recommend you to use class MemoryPool. Use new to allocate them from heap.
    // remember, you can not use any containers with heap allocations
    class FreeListMemoryPool {
        friend class FreeListAllocator;

        struct Block {
            Block *next;
            size_t size;
        };
        static constexpr Block *OCCUPIED_BLOCK = nullptr;

    public:
        FreeListMemoryPool() noexcept
        {
            freeListHead_->size = CalculateCapacity() - sizeof(FreeListMemoryPool);
            freeListHead_->next = freeListHead_ + 1;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        char *Allocate(size_t size) noexcept
        {
            Block *prevBlock = nullptr;
            Block *block = freeListHead_;

            while (block && block->size + sizeof(Block) < size) {
                prevBlock = block;
                block = block->next;
            }

            if (block == nullptr) {
                return nullptr;
            }

            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            auto allocated = reinterpret_cast<char *>(block) + sizeof(Block);

            Block *nextFreeBlock = block->next;

            if (block->size < size + sizeof(Block)) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                nextFreeBlock = reinterpret_cast<Block *>(allocated + size);
                nextFreeBlock->size = block->size - size - sizeof(Block);
                nextFreeBlock->next = block->next;
            }

            if (prevBlock) {
                prevBlock->next = nextFreeBlock;
            }

            if (freeListHead_ == block) {
                freeListHead_ = nextFreeBlock;
            }

            block->next = OCCUPIED_BLOCK;
            block->size = size + sizeof(Block);

            return allocated;
        }

        void Free(void *ptr) noexcept
        {
            if (ptr == nullptr) {
                return;
            }

            if (!VerifyPtr(ptr)) {
                return;
            }

            Block *block = reinterpret_cast<Block *>(ptr);

            block->next = freeListHead_;
            freeListHead_ = block;
        }

        bool VerifyPtr(const void *ptr) const noexcept
        {
            if (ptr == nullptr) {
                return false;
            }

            auto block = reinterpret_cast<const Block *>(ptr);

            if (block->next != OCCUPIED_BLOCK) {
                return false;
            }

            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            auto base = reinterpret_cast<const char *>(this);
            return this <= ptr && reinterpret_cast<const char *>(ptr) + sizeof(Block) < base + CalculateCapacity();
        }

    private:
        // NOLINTNEXTLINE(modernize-use-nullptr)
        FreeListMemoryPool *nextPool = nullptr;
        Block *freeListHead_ = reinterpret_cast<Block *>(this + sizeof(FreeListMemoryPool));
    };

public:
    FreeListAllocator()
    {
        if (firstPool_ == nullptr) {
            throw std::bad_alloc();
        }
    }

    ~FreeListAllocator() noexcept
    {
        munmap(firstPool_, CalculateCapacity());
    }

    NO_MOVE_SEMANTIC(FreeListAllocator);
    NO_COPY_SEMANTIC(FreeListAllocator);

    template <class T = char>
    T *Allocate(size_t count) noexcept
    {
        MemPool *pool = firstPool_;

        size_t size = count * sizeof(T);

        char *allocated = pool->Allocate(size);

        while (allocated == nullptr) {
            MemPool *prevPool = pool;
            pool = pool->nextPool;

            if (pool == nullptr) {
                pool = CreateNewPool();
                prevPool->nextPool = pool;
            }

            allocated = pool->Allocate(size);
        }

        return reinterpret_cast<T *>(allocated);
    }

    void Free(void *ptr) noexcept
    {
        MemPool *pool = firstPool_;

        while (pool) {
            pool = pool->nextPool;
            if (pool->VerifyPtr(ptr)) {
                pool->Free(ptr);
                break;
            }
        }
    }

    /**
     * @brief Method should check in @param ptr is pointer to mem from this allocator
     * @returns true if ptr is from this allocator
     */
    bool VerifyPtr(void *ptr)
    {
        return false;
    }

private:
    using MemPool = FreeListMemoryPool;
    MemPool *firstPool_ = CreateNewPool();

    static MemPool *CreateNewPool() noexcept
    {
        return reinterpret_cast<MemPool *>(
            mmap(NULL, CalculateCapacity(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    }

    static size_t CalculateCapacity() noexcept
    {
        size_t psize = getpagesize();
        size_t cap = psize * (1 + ONE_MEM_POOL_SIZE / psize);
        if (ONE_MEM_POOL_SIZE % psize == 0) {
            cap -= psize;
        }
        return cap;
    }
};

#endif  // MEMORY_MANAGEMENT_FREE_LIST_ALLOCATOR_INCLUDE_FREE_LIST_ALLOCATOR_H
