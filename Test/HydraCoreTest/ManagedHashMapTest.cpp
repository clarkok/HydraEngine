#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "TestHeapObject.h"
#include "Runtime/ManagedHashMap.h"

namespace hydra
{

using TestMap = runtime::HashMap<TestHeapObject *>;
using runtime::String;

TEST_CASE("ManagedHashMap", "[Runtime]")
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
            REQUIRE(result);

            TestHeapObject *fetched = nullptr;
            result = uut->Find(TEST_KEY, fetched);
            REQUIRE(result);
            REQUIRE(fetched == value);

            result = uut->Remove(TEST_KEY, value);
            REQUIRE(result);

            result = uut->Find(TEST_KEY, fetched);
            REQUIRE(!result);
        }
    }

    SECTION("Find Or Set")
    {
        auto value = allocator.AllocateAuto<TestHeapObject>();

        auto result = uut->FindOrSet(TEST_KEY, value);
        REQUIRE(result);

        TestHeapObject *fetched = nullptr;
        result = uut->Find(TEST_KEY, fetched);
        REQUIRE(result);
        REQUIRE(fetched == value);

        auto otherValue = allocator.AllocateAuto<TestHeapObject>();
        result = uut->FindOrSet(TEST_KEY, otherValue);
        REQUIRE(!result);
        REQUIRE(otherValue == value);

        result = uut->Remove(TEST_KEY, value);
        REQUIRE(result);
        otherValue = allocator.AllocateAuto<TestHeapObject>();
        result = uut->FindOrSet(TEST_KEY, otherValue);
        REQUIRE(result);

        result = uut->Find(TEST_KEY, fetched);
        REQUIRE(result);
        REQUIRE(fetched == otherValue);
    }

    SECTION("Enlarge")
    {
        std::vector<TestHeapObject *> values;

        for (size_t i = 0; i < uut->Capacity(); ++i)
        {
            INFO(i);

            auto key = String::Slice(allocator, TEST_KEY, i, 1);
            auto value = allocator.AllocateAuto<TestHeapObject>();

            auto result = uut->TrySet(key, value);
            REQUIRE(result);

            values.push_back(value);
        }

        for (size_t i = 0; i < uut->Capacity(); ++i)
        {
            INFO(i);

            auto key = String::Slice(allocator, TEST_KEY, i, 1);
            TestHeapObject *value = nullptr;

            auto result = uut->Find(key, value);
            REQUIRE(result);
            REQUIRE(value == values[i]);
        }

        auto original = uut;

        auto enlarged = uut->Rehash(allocator);
        REQUIRE(enlarged != nullptr);
        REQUIRE(enlarged->Capacity() > uut->Capacity());

        auto enlargeAgain = uut->Rehash(allocator);
        REQUIRE(enlarged == enlargeAgain);

        auto latest = TestMap::Latest(uut);
        REQUIRE(latest == enlarged);
        REQUIRE(uut == enlarged);

        for (size_t i = 0; i < original->Capacity(); ++i)
        {
            INFO(i);

            auto key = String::Slice(allocator, TEST_KEY, i, 1);
            TestHeapObject *value = nullptr;

            auto result = enlarged->Find(key, value);
            REQUIRE(result);
            REQUIRE(value == values[i]);
        }
    }
}

TEST_CASE("ManagedHashMapGCTest", "[Runtime][!hide]")
{
    gc::Heap *heap = gc::Heap::GetInstance();
    gc::ThreadAllocator allocator(heap);
    auto uut = TestMap::New(allocator, 20);

    auto TEST_KEY = String::New(allocator,
        u"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890");

    REQUIRE(uut->Capacity() == 40);
    std::array<TestHeapObject *, 40> values;
    std::fill(values.begin(), values.end(), nullptr);

    while (true)
    {
        for (size_t i = 0; i < 40; ++i)
        {
            auto key = String::Slice(allocator, TEST_KEY, i, 1);
            auto value = allocator.AllocateAuto<TestHeapObject>();

            auto result = uut->TrySet(key, value);
            // REQUIRE(result);
            hydra_assert(result, "REQUIRE");

            values[i] = value;
        }

        for (size_t i = 0; i < 40; ++i)
        {
            auto key = String::Slice(allocator, TEST_KEY, i, 1);
            TestHeapObject *value = nullptr;

            auto result = uut->Find(key, value);
            // REQUIRE(result);
            // REQUIRE(value == values[i]);
            hydra_assert(result, "REQUIRE");
            hydra_assert(value == values[i], "REQUIRE");
        }
    }
}

TEST_CASE("ManagedHashMultiThreadsTest", "[Runtime][!hide]")
{
    gc::Heap *heap = gc::Heap::GetInstance();
    gc::ThreadAllocator allocator(heap);
    auto uut = TestMap::New(allocator, 20);

    auto ALPHABET = String::New(allocator,
        u"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

    REQUIRE(uut->Capacity() == 40);

    auto RandomKey = [&](gc::ThreadAllocator &allocator, size_t length) -> String*
    {
        String *ret = String::Empty(allocator);
        while (length--)
        {
            ret = String::Concat(allocator,
                ret,
                String::Slice(allocator, ALPHABET, std::rand() % ALPHABET->length(), 1));
        }

        return ret;
    };

    /*
    std::thread otherThread([&]()
    {
        gc::ThreadAllocator allocator(heap);

        for (;;) {
            auto value = allocator.AllocateAuto<TestHeapObject>();
            auto key = RandomKey(allocator, 16);
            TestMap::Latest(uut)->SetAuto(allocator, key, value);

            TestHeapObject *fetched = nullptr;
            auto result = TestMap::Latest(uut)->Find(key, fetched);
            hydra_assert(result, "REQUIRE");
            hydra_assert(fetched == value, "REQUIRE");

            TestMap::Latest(uut)->Remove(key, fetched);
        }
    });
    */

    for (;;) {
        auto value = allocator.AllocateAuto<TestHeapObject>();
        auto key = RandomKey(allocator, 15);
        TestMap::Latest(uut)->SetAuto(allocator, key, value);

        TestHeapObject *fetched = nullptr;
        auto result = TestMap::Latest(uut)->Find(key, fetched);
        hydra_assert(result, "REQUIRE");
        hydra_assert(fetched == value, "REQUIRE");

        TestMap::Latest(uut)->Remove(key, fetched);
    }
}

}