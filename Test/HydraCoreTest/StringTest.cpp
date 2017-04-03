#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "Runtime/String.h"

namespace hydra
{

TEST_CASE("String basic", "Runtime")
{
    char_t TEST_TEXT[] = u"hahahahahohohoho";

    gc::ThreadAllocator allocator(gc::Heap::GetInstance());

    runtime::String *uutA = runtime::String::New(
        allocator,
        std::begin(TEST_TEXT),
        std::end(TEST_TEXT)
    );

    REQUIRE(uutA != nullptr);
    REQUIRE(uutA->length() == std::distance(std::begin(TEST_TEXT), std::end(TEST_TEXT)));
    for (size_t i = 0; i < uutA->length(); ++i)
    {
        REQUIRE(uutA->at(i) == TEST_TEXT[i]);
    }

    runtime::String *uutEmpty = runtime::String::Empty(
        allocator,
        [=]()
        {
            gc::Heap::GetInstance()->Remember(uutA);
        });

    REQUIRE(uutEmpty != nullptr);
}

}