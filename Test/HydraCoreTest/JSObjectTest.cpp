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
    JSObject *uut = emptyKlass->NewObject(allocator);

    SECTION("Basic Test")
    {
        String *key = String::New(allocator, u"key");
        JSValue value = JSValue::FromNumber(10);
        JSValue fetched;
        bool result = false;

        result = uut->Has(key);
        fetched = uut->Get(key);
        REQUIRE(!result);
        REQUIRE(fetched == JSValue());

        uut->Add(allocator, key, value);
        auto originalUut = uut;
        REQUIRE(JSObject::Latest(uut) == originalUut);

        result = uut->Has(key);
        REQUIRE(result);

        fetched = uut->Get(key);
        REQUIRE(fetched == value);

        uut->Delete(key);
        result = uut->Has(key);
        fetched = uut->Get(key);
        REQUIRE(!result);
        REQUIRE(fetched == JSValue());
    }
}

}