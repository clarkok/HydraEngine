#include "catch.hpp"

#include "runtime/memory.hpp"

#include <iostream>

using namespace hydra;
using namespace memory;

TEST_CASE("AlignAlloc will allocate aligned memory", "[Memory]")
{
#define TEST_ALIGN_HELPER(_varname, _size, _align, _mask)               \
    {                                                                   \
        void *_varname = AlignAlloc(_size, _align);                     \
        REQUIRE((reinterpret_cast<uint64_t>(_varname) & _mask) == 0);   \
        AlignFree(_varname);                                            \
    }

#define TEST_ALIGN(_bits)           \
    TEST_ALIGN_HELPER(align_##_bits, (1 << _bits), (1 << _bits), ((1 << _bits) - 1))

    TEST_ALIGN(10); // 1KB
    TEST_ALIGN(11);
    TEST_ALIGN(12);
    TEST_ALIGN(13);
    TEST_ALIGN(14);
    TEST_ALIGN(15);
    TEST_ALIGN(16);
    TEST_ALIGN(17);
    TEST_ALIGN(18);
    TEST_ALIGN(19);
    TEST_ALIGN(20); // 1MB
}

TEST_CASE("WriteProtect will protect memory against modify", "[Memory]")
{
    const size_t ALLOC_SIZE = (1 << 20);    // 1MB
    volatile void *ptr = AlignAlloc(ALLOC_SIZE, ALLOC_SIZE);
    volatile void* accessed = nullptr;

    uint64_t handle = AddMemoryWriteWatcher([&](volatile void* addr)
    {
        accessed = addr;
        WriteUnprotect(ptr, ALLOC_SIZE);
        return true;
    });

    *reinterpret_cast<volatile size_t*>(ptr) = ALLOC_SIZE;
    REQUIRE(*reinterpret_cast<volatile size_t*>(ptr) == ALLOC_SIZE);

    WriteProtect(ptr, ALLOC_SIZE);

    *reinterpret_cast<volatile size_t*>(ptr) = ALLOC_SIZE / 2;
    REQUIRE(*reinterpret_cast<volatile size_t*>(ptr) == ALLOC_SIZE / 2);

    REQUIRE(accessed == ptr);

    AlignFree((void*)ptr);
    RemoveMemoryWriteWatcher(handle);
}

TEST_CASE("MemoryWriteWatcher can be removed", "[Memory]")
{
    const size_t ALLOC_SIZE = (1 << 20);    // 1MB
    volatile void *ptr = AlignAlloc(ALLOC_SIZE, ALLOC_SIZE);
    bool accessed = false;

    uint64_t handle = AddMemoryWriteWatcher([&](volatile void* addr)
    {
        accessed = true;
        WriteUnprotect(ptr, ALLOC_SIZE);
        return true;
    });

    WriteProtect(ptr, ALLOC_SIZE);
    *reinterpret_cast<volatile size_t*>(ptr) = ALLOC_SIZE;

    REQUIRE(accessed);
    accessed = false;

    RemoveMemoryWriteWatcher(handle);

    handle = AddMemoryWriteWatcher([&](volatile void *addr)
    {
        WriteUnprotect(ptr, ALLOC_SIZE);
        return true;
    });

    WriteProtect(ptr, ALLOC_SIZE);
    *reinterpret_cast<volatile size_t*>(ptr) = ALLOC_SIZE;

    REQUIRE(!accessed);

    AlignFree((void*)ptr);
    RemoveMemoryWriteWatcher(handle);
}