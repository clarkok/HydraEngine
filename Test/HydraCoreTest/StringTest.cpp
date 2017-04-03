#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "Runtime/String.h"

namespace hydra
{

char_t TEST_TEXT[] = u"hahahahahohohoho";
size_t TEST_LENGTH = sizeof(TEST_TEXT) / sizeof(TEST_TEXT[0]) - 1;

TEST_CASE("String basic", "Runtime")
{
    gc::ThreadAllocator allocator(gc::Heap::GetInstance());

    SECTION("Test ManagedString")
    {
        runtime::String *uut = runtime::String::New(
            allocator,
            std::begin(TEST_TEXT),
            std::begin(TEST_TEXT) + TEST_LENGTH
        );

        REQUIRE(uut != nullptr);
        REQUIRE(uut->length() == TEST_LENGTH);
        for (size_t i = 0; i < uut->length(); ++i)
        {
            REQUIRE(uut->at(i) == TEST_TEXT[i]);
        }
        REQUIRE_THROWS(uut->at(10000));
    }

    SECTION("Test EmptyString")
    {
        runtime::String *uutEmpty = runtime::String::Empty(allocator);

        REQUIRE(uutEmpty != nullptr);
        REQUIRE(uutEmpty->length() == 0);
        REQUIRE_THROWS(uutEmpty->at(1));
    }

    SECTION("Test ConcatedString")
    {
        runtime::String *stringSliced = runtime::String::New(
            allocator,
            std::begin(TEST_TEXT),
            std::begin(TEST_TEXT) + TEST_LENGTH
        );

        runtime::String *uut = runtime::String::Concat(
            allocator,
            stringSliced,
            stringSliced
        );

        REQUIRE(uut != nullptr);
        REQUIRE(uut->length() == TEST_LENGTH * 2);
        for (size_t i = 0; i < uut->length(); ++i)
        {
            INFO(i);
            REQUIRE(uut->at(i) == TEST_TEXT[i % TEST_LENGTH]);
        }
        REQUIRE_THROWS(uut->at(100000));
    }

    SECTION("Test SlicedString")
    {
        runtime::String *str = runtime::String::New(
            allocator,
            std::begin(TEST_TEXT),
            std::begin(TEST_TEXT) + TEST_LENGTH
        );

        runtime::String *uut = runtime::String::Slice(
            allocator,
            str,
            4,
            8
        );

        REQUIRE(uut != nullptr);
        REQUIRE(uut->length() == 8);
        for (size_t i = 0; i < uut->length(); ++i)
        {
            INFO(i);
            REQUIRE(uut->at(i) == TEST_TEXT[i + 4]);
        }
        REQUIRE_THROWS(uut->at(9));
    }
}

}