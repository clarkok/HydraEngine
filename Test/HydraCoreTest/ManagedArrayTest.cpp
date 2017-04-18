#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "Runtime/ManagedArray.h"

namespace hydra
{

using runtime::ManagedArray;
using runtime::JSValue;

TEST_CASE("ManagedArray", "[Runtime]")
{
    gc::Heap *heap = gc::Heap::GetInstance();
    gc::ThreadAllocator allocator(heap);

    ManagedArray *uut = ManagedArray::New(allocator, 16);
    REQUIRE(uut->Capacity() >= 16);

    for (auto value : *uut)
    {
        REQUIRE(value == JSValue());
    }
}

}