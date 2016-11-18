#include "catch.hpp"

#include "runtime/memory.hpp"

using namespace hydra;

TEST_CASE("AlignAlloc will allocate aligned memory", "[AlignAlloc]")
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

TEST_CASE("WriteProtect will protect memory against modify", "[WriteProtect]")
{
    const size_t ALLOC_SIZE = (1 << 20);    // 1MB
    volatile void *ptr = AlignAlloc(ALLOC_SIZE, ALLOC_SIZE);
    void* accessed = nullptr;

    SetMemoryWriteWatcher([&](void* addr)
    {
        accessed = addr;
        WriteUnprotect((void*)ptr, ALLOC_SIZE);
        return true;
    });

    *reinterpret_cast<volatile size_t*>(ptr) = ALLOC_SIZE;
    REQUIRE(*reinterpret_cast<volatile size_t*>(ptr) == ALLOC_SIZE);

    WriteProtect((void*)ptr, ALLOC_SIZE);

    *reinterpret_cast<volatile size_t*>(ptr) = ALLOC_SIZE / 2;
    REQUIRE(*reinterpret_cast<volatile size_t*>(ptr) == ALLOC_SIZE / 2);

    WriteUnprotect((void*)ptr, ALLOC_SIZE);

    REQUIRE(accessed == ptr);

    AlignFree((void*)ptr);
}