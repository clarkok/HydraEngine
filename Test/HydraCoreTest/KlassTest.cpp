#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "Runtime/Klass.h"

namespace hydra
{

using runtime::Klass;
using runtime::String;

TEST_CASE("Klass", "[Runtime]")
{
    auto heap = gc::Heap::GetInstance();
    gc::ThreadAllocator allocator(heap);

    auto emptyKlass = Klass::EmptyKlass(allocator);
    REQUIRE(emptyKlass->size() == 0);

    SECTION("AddTransaction Basic")
    {
        auto newKlass = emptyKlass->AddTransaction(
            allocator, String::New(allocator, u"a"));
        REQUIRE(newKlass->size() == 1);
        REQUIRE(String::New(allocator, u"a")->EqualsTo(*(newKlass->begin())));
    }

    SECTION("AddTransaction Twice")
    {
        auto newKlass = emptyKlass->AddTransaction(
            allocator, String::New(allocator, u"a"));

        auto otherKlass = emptyKlass->AddTransaction(
            allocator, String::New(allocator, u"a"));

        REQUIRE(newKlass == otherKlass);
    }

    SECTION("Find Basic")
    {
        auto uut = emptyKlass->AddTransaction(
            allocator, String::New(allocator, u"a"));

        uut = uut->AddTransaction(
            allocator, String::New(allocator, u"b"));;

        {
            size_t index = 0;
            auto result = uut->Find(String::New(allocator, u"a"), index);
            REQUIRE(result);
            REQUIRE(index == 0);
        }

        {
            size_t index = 0;
            auto result = uut->Find(String::New(allocator, u"b"), index);
            REQUIRE(result);
            REQUIRE(index == 1);
        }
    }

    SECTION("Find HashMap")
    {
        auto ALPHABET = String::New(allocator, u"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        auto uut = emptyKlass;

        REQUIRE(26 > runtime::MINIMAL_KEY_COUNT_TO_ENABLE_HASH_IN_KLASS);

        for (size_t i = 0; i < 26; ++i)
        {
            auto key = String::Slice(allocator, ALPHABET, i, 1);
            uut = uut->AddTransaction(allocator, key);

            for (size_t j = 0; j <= i; ++j)
            {
                auto key = String::Slice(allocator, ALPHABET, j, 1);
                size_t index = 0;
                auto result = uut->Find(key, index);
                REQUIRE(result);
                REQUIRE(index == j);
            }

            for (size_t j = i + 1; j < 26; ++j)
            {
                auto key = String::Slice(allocator, ALPHABET, j, 1);
                size_t index = 0;
                auto result = uut->Find(key, index);
                REQUIRE(!result);
            }
        }
    }
}

}