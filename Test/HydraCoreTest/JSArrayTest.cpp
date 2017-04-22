#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp" 

#include "Runtime/JSArray.h"

namespace hydra
{

using runtime::Klass;
using runtime::JSArray;
using runtime::JSObjectPropertyAttribute;
using runtime::JSValue;

TEST_CASE("JSArrayTest", "[Runtime]")
{
    gc::Heap *heap = gc::Heap::GetInstance();
    gc::ThreadAllocator allocator(heap);

    auto uut = JSArray::New(allocator);

    JSValue fetched;
    JSObjectPropertyAttribute attribute;
    bool result = false;

    SECTION("Basic Test")
    {
        JSValue value = JSValue::FromNumber(10);

        result = uut->Get(0, fetched, attribute);
        REQUIRE(!result);

        uut->Set(allocator, 0, value);

        result = uut->Get(0, fetched, attribute);
        REQUIRE(result);
        REQUIRE(fetched == value);
        REQUIRE(attribute == JSObjectPropertyAttribute::DEFAULT_DATA_ATTRIBUTE);

        uut->Delete(0);

        result = uut->Get(0, fetched, attribute);
        REQUIRE(!result);
    }

    SECTION("Slow Test")
    {
        for (size_t i = runtime::DEFAULT_JSARRAY_SPLIT_POINT * 4;
            i > runtime::DEFAULT_JSARRAY_SPLIT_POINT * 2;
            --i)
        {
            INFO(i);

            JSValue value = JSValue::FromNumber(i);

            result = uut->Get(i, fetched, attribute);
            REQUIRE(!result);

            uut->Set(allocator, i, value);

            for (auto j = i; j <= runtime::DEFAULT_JSARRAY_SPLIT_POINT * 4; ++j)
            {
                result = uut->Get(j, fetched, attribute);
                REQUIRE(result);
                REQUIRE(fetched == JSValue::FromNumber(j));
                REQUIRE(attribute == JSObjectPropertyAttribute::DEFAULT_DATA_ATTRIBUTE);
            }
        }

        for (size_t i = runtime::DEFAULT_JSARRAY_SPLIT_POINT * 4;
            i > runtime::DEFAULT_JSARRAY_SPLIT_POINT * 2;
            --i)
        {
            INFO(i);

            uut->Delete(i);

            result = uut->Get(i, fetched, attribute);
            REQUIRE(!result);
        }

        REQUIRE(uut->GetSplitPoint() == runtime::DEFAULT_JSARRAY_SPLIT_POINT);
    }

    SECTION("Move split point")
    {
        for (size_t i = runtime::DEFAULT_JSARRAY_SPLIT_POINT;
            i < runtime::DEFAULT_JSARRAY_SPLIT_POINT * 2;
            ++i)
        {
            INFO(i);

            JSValue value = JSValue::FromNumber(i);

            uut->Set(allocator, i, value);

            result = uut->Get(i, fetched, attribute);
            REQUIRE(result);
            REQUIRE(fetched == value);
            REQUIRE(attribute == JSObjectPropertyAttribute::DEFAULT_DATA_ATTRIBUTE);

            REQUIRE(uut->GetSplitPoint() == i + 1);
        }
    }

    SECTION("Move split point backward")
    {
        for (size_t i = runtime::DEFAULT_JSARRAY_SPLIT_POINT;
            i < runtime::DEFAULT_JSARRAY_SPLIT_POINT * 2;
            ++i)
        {
            JSValue value = JSValue::FromNumber(i);
            uut->Set(allocator, i, value);
        }

        for (size_t i = runtime::DEFAULT_JSARRAY_SPLIT_POINT * 2 - 1;
            i >= runtime::DEFAULT_JSARRAY_SPLIT_POINT;
            --i)
        {
            INFO(i);

            uut->Delete(i);

            REQUIRE(uut->GetSplitPoint() == i);
        }
    }

    SECTION("Move split point by 2")
    {
        for (size_t i = runtime::DEFAULT_JSARRAY_SPLIT_POINT;
            i < runtime::DEFAULT_JSARRAY_SPLIT_POINT * 2;
            i += 2)
        {
            INFO(i);

            JSValue value = JSValue::FromNumber(i);

            uut->Set(allocator, i, value);

            result = uut->Get(i, fetched, attribute);
            REQUIRE(result);
            REQUIRE(fetched == value);
            REQUIRE(attribute == JSObjectPropertyAttribute::DEFAULT_DATA_ATTRIBUTE);

            REQUIRE(uut->GetSplitPoint() == i + 1);
        }
    }
}

}