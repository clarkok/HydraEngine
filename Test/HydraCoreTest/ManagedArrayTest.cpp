#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "Runtime/ManagedArray.h"

namespace hydra
{

using runtime::Array;
using runtime::JSValue;

TEST_CASE("Array", "[Runtime]")
{
    gc::Heap *heap = gc::Heap::GetInstance();
    gc::ThreadAllocator allocator(heap);

    Array *uut = Array::New(allocator, 16);
    REQUIRE(uut->Capacity() >= 16);

    for (auto value : *uut)
    {
        REQUIRE(value == JSValue());
    }
}

}