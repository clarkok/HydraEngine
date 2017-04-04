#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "TestHeapObject.h"
#include "Runtime/ManagedHashMap.h"

namespace hydra
{

using TestMap = runtime::HashMap<TestHeapObject *>;
using runtime::String;

TEST_CASE("ManagedHashMap", "Runtime")
{
    gc::Heap *heap = gc::Heap::GetInstance();
    gc::ThreadAllocator allocator(heap);
    auto uut = TestMap::New(allocator, 20);

    auto TEST_KEY = String::New(allocator,
        u"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890");

    REQUIRE(uut->Capacity() == 40);

    SECTION("Basic")
    {
        auto value = allocator.AllocateAuto<TestHeapObject>();
        auto expectedValue = value;
        auto valueId = value->Id;

        auto result = uut->TrySet(TEST_KEY, value);
        REQUIRE(result);

        value = nullptr;
        result = uut->Find(TEST_KEY, value);
        REQUIRE(result);
        REQUIRE(value == expectedValue);

        for (size_t i = 1; i < TEST_KEY->length() - 1; ++i)
        {
            auto key = String::Slice(allocator, TEST_KEY, i, TEST_KEY->length() - i);
            auto result = uut->Find(key, value);
            REQUIRE(!result);
        }

        result = uut->Remove(TEST_KEY, value);
        REQUIRE(result);

        result = uut->Find(TEST_KEY, value);
        REQUIRE(!result);
    }

    SECTION("Delete Mark")
    {
        auto value = allocator.AllocateAuto<TestHeapObject>();

        for (size_t i = 0; i < uut->Capacity() * 2; ++i)
        {
            auto result = uut->TrySet(TEST_KEY, value);
            TestHeapObject *fetched = nullptr;
            REQUIRE(result);

            result = uut->Find(TEST_KEY, fetched);
            REQUIRE(result);
            REQUIRE(fetched == value);

            result = uut->Remove(TEST_KEY, value);
            REQUIRE(result);

            result = uut->Find(TEST_KEY, fetched);
            REQUIRE(!result);
        }
    }
}

}