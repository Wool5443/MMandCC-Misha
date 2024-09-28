
#include <gtest/gtest.h>
#include <cstddef>
#include "bump_pointer_allocator/include/bump_pointer_allocator.h"

TEST(BumpAllocatorTest, DISABLED_TemplateAllocationTest)  // remove DISABLED_ prefix to use test
{
    constexpr size_t MemoryPoolSize = 4048U;
    BumpPointerAllocator<MemoryPoolSize> allocator;

    constexpr size_t COUNT_OF_SIZE = 10U;
    auto *first = allocator.Allocate<size_t>(COUNT_OF_SIZE);
    ASSERT_NE(first, nullptr);
    ASSERT_TRUE(allocator.VerifyPtr(first));

    auto *second = allocator.Allocate<size_t>(1U);
    ASSERT_NE(second, nullptr);
    ASSERT_TRUE(allocator.VerifyPtr(second));
    // check that this allocator works as bump_pointer
    ASSERT_EQ(size_t(second) - size_t(first), COUNT_OF_SIZE * sizeof(size_t));

    ASSERT_EQ(allocator.Allocate<char>(0), nullptr);  // you can not allocate memory with 0 size
}

TEST(BumpAllocatorTest, DISABLED_AllocatorMemPoolOverflowTest)  // remove DISABLED_ prefix to use test
{
    constexpr size_t MemoryPoolSize = 64U;
    BumpPointerAllocator<MemoryPoolSize> allocator;

    constexpr size_t COUNT_TO_ALLOC = 63U;
    auto *mem = allocator.Allocate<char>(COUNT_TO_ALLOC);
    ASSERT_TRUE(allocator.VerifyPtr(mem));
    ASSERT_FALSE(allocator.VerifyPtr(mem + 1U));  // in should be false becase you can not allocate mem with 0 size

    ASSERT_EQ(allocator.Allocate<char>(5U), nullptr);
    allocator.Free();
    ASSERT_EQ(allocator.Allocate<char>(5U), mem);
    allocator.Free();
    ASSERT_EQ(allocator.Allocate<size_t>(10U), nullptr);
}