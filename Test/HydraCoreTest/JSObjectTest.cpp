#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "Runtime/JSObject.h"
#include "Runtime/Klass.h"
#include "Runtime/Type.h"

namespace hydra
{

using runtime::Klass;
using runtime::JSObject;
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
    bool result = false;

    SECTION("Basic Test")
    {
        JSValue value = JSValue::FromNumber(10);

        fetched = uut->Get(key);
        REQUIRE(!result);
        REQUIRE(fetched == JSValue());

        uut->Add(allocator, key, value);
        auto originalUut = uut;
        REQUIRE(JSObject::Latest(uut) == originalUut);

        fetched = uut->Get(key);
        REQUIRE(fetched == value);

        uut->Delete(key);
        fetched = uut->Get(key);
        REQUIRE(fetched == JSValue());
    }

    SECTION("Undefined Test")
    {
        JSValue undefined = JSValue::Undefined();

        fetched = uut->Get(key);
        REQUIRE(!result);
        REQUIRE(fetched == JSValue());

        uut->Add(allocator, key, undefined);
        fetched = uut->Get(key);
        REQUIRE(fetched == undefined);

        uut->Delete(key);
        fetched = uut->Get(key);
        REQUIRE(fetched == JSValue());

        result = uut->Set(key, undefined);
        REQUIRE(result);

        fetched = uut->Get(key);
        REQUIRE(fetched == undefined);
    }

    SECTION("Enlarge Test")
    {
        String *ALPHABET = String::New(allocator, u"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        auto originalUut = uut;

        for (size_t i = 0; i < ALPHABET->length(); ++i)
        {
            auto key = String::Slice(allocator, ALPHABET, i, 1);
            auto value = JSValue::FromSmallInt(i);

            JSObject::Latest(uut)->Add(allocator, key, value);
        }

        REQUIRE(uut != originalUut);

        for (size_t i = 0; i < ALPHABET->length(); ++i)
        {
            auto key = String::Slice(allocator, ALPHABET, i, 1);

            REQUIRE(uut->Get(key) == JSValue::FromSmallInt(i));
        }
    }
}

}