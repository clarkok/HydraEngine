#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "Runtime/JSObject.h"
#include "Runtime/Klass.h"
#include "Runtime/Type.h"

namespace hydra
{

using runtime::Klass;
using runtime::JSObject;
using runtime::JSObjectPropertyAttribute;
using runtime::JSValue;
using runtime::String;

TEST_CASE("JSObjectTest", "[Runtime]")
{
    gc::Heap *heap = gc::Heap::GetInstance();
    gc::ThreadAllocator allocator(heap);

    Klass *emptyKlass = Klass::EmptyKlass(allocator);
    JSObject *uut = emptyKlass->NewObject<JSObject>(allocator);
    String *key = String::New(allocator, u"key");
    JSValue fetched;
    JSObjectPropertyAttribute attribute;
    bool result = false;

    SECTION("Basic Test")
    {
        JSValue value = JSValue::FromNumber(10);

        result = uut->Get(key, fetched, attribute);
        REQUIRE(!result);

        uut->Set(allocator, key, value);

        result = uut->Get(key, fetched, attribute);
        REQUIRE(result);
        REQUIRE(fetched == value);
        REQUIRE(attribute == JSObjectPropertyAttribute::DEFAULT_DATA_ATTRIBUTE);

        uut->Delete(key);
        result = uut->Get(key, fetched, attribute);
        REQUIRE(!result);
    }

    SECTION("Undefined Test")
    {
        JSValue undefined = JSValue::Undefined();

        result = uut->Get(key, fetched, attribute);
        REQUIRE(!result);

        uut->Set(allocator, key, undefined);

        result = uut->Get(key, fetched, attribute);
        REQUIRE(result);
        REQUIRE(fetched == undefined);
        REQUIRE(attribute == JSObjectPropertyAttribute::DEFAULT_DATA_ATTRIBUTE);

        uut->Delete(key);

        result = uut->Get(key, fetched, attribute);
        REQUIRE(!result);

        uut->Set(allocator, key, undefined);

        result = uut->Get(key, fetched, attribute);
        REQUIRE(result);
        REQUIRE(fetched == undefined);
        REQUIRE(attribute == JSObjectPropertyAttribute::DEFAULT_DATA_ATTRIBUTE);
    }

    SECTION("Enlarge Test")
    {
        String *ALPHABET = String::New(allocator, u"ABCDEFGHIJKLMNOPQRSTUVWXYZ");

        for (size_t i = 0; i < ALPHABET->length(); ++i)
        {
            auto key = String::Slice(allocator, ALPHABET, i, 1);
            auto value = JSValue::FromSmallInt(i);

            uut->Set(allocator, key, value);
        }

        for (size_t i = 0; i < ALPHABET->length(); ++i)
        {
            auto key = String::Slice(allocator, ALPHABET, i, 1);
            auto result = uut->Get(key, fetched, attribute);

            REQUIRE(result);
            REQUIRE(fetched == JSValue::FromSmallInt(i));
            REQUIRE(attribute == JSObjectPropertyAttribute::DEFAULT_DATA_ATTRIBUTE);
        }
    }

    SECTION("Index and Offset Test")
    {
        String *ALPHABET = String::New(allocator, u"ABCDEFGHIJKLMNOPQRSTUVWXYZ");

        for (size_t i = 0; i < ALPHABET->length(); ++i)
        {
            auto key = String::Slice(allocator, ALPHABET, i, 1);
            auto value = JSValue::FromSmallInt(i);

            uut->Set(allocator, key, value);
        }

        auto pTable = JSObject::GetTable();

        auto visitUutByIndex = [&](size_t index) -> JSValue
        {
            return (uut->*pTable)->at((index << 1) + 1);
        };

        for (size_t i = 0; i < ALPHABET->length(); ++i)
        {
            auto key = String::Slice(allocator, ALPHABET, i, 1);
            auto expected = JSValue::FromSmallInt(i);

            result = uut->Get(key, fetched, attribute);
            REQUIRE(result);
            REQUIRE(fetched == expected);

            size_t index = 0;
            result = uut->Index(key, index);
            REQUIRE(result);
            REQUIRE(visitUutByIndex(index) == expected);
        }
    }
}

}
