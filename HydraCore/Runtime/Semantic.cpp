#include "Semantic.h"

#include "JSFunction.h"

#include "Klass.h"

namespace hydra
{
namespace runtime
{
namespace semantic
{

#define STRING_LIST(def)                \
    def(u"__proto__", __PROTO__)        \
    def(u"prototype", PROTOTYPE)        \
    def(u"constructor", CONSTRUCTOR)    \
    def(u"length", LENGTH)              \
    def(u"get", GET)                    \
    def(u"set", SET)                    \


static bool lib_Object(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error);
static bool lib_Function(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error);
static bool lib_IsArray(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error);
static bool lib_isArraySafe(JSValue value);

namespace strs
{

#define DECL_VAR(__value, __name)   static String *__name;
STRING_LIST(DECL_VAR)
#undef DECL_VAR

static inline void Initialize(gc::ThreadAllocator &allocator)
{
#define INIT_VAR(__value, __name)   __name = String::New(allocator, __value);
    STRING_LIST(INIT_VAR)
#undef INIT_VAR
}

static inline void Scan(std::function<void(gc::HeapObject*)> scan)
{
#define SCAN_VAR(__value, __name)   scan(__name);
    STRING_LIST(SCAN_VAR)
#undef SCAN_VAR
}

}

#define js_throw_error(type, msg)   hydra_assert(false, "runtime error")

// Klasses
static Klass *emptyKlass;
static Klass *emptyObjectKlass;
static size_t expectedObjectProtoOffset;
static size_t expectedObjectConstructorOffset;

static Klass *expectedArrayKlass;
static size_t expectedArrayLengthOffset;

// Objects
static JSObject *Object;
static JSObject *Object_prototype;
static JSObject *Function;
static JSObject *Function_prototype;
static JSObject *Array;

void RootScan(std::function<void(gc::HeapObject*)> scan)
{
    strs::Scan(scan);

    scan(emptyKlass);
    scan(emptyObjectKlass);
    scan(expectedArrayKlass);

    scan(Object);
    scan(Object_prototype);
    scan(Function);
    scan(Function_prototype);

    /*
    scan(Array);
    */
}

void Initialize(gc::ThreadAllocator &allocator)
{
    strs::Initialize(allocator);

    bool result;
    emptyKlass = Klass::EmptyKlass(allocator);

    // Klasses
    emptyObjectKlass = emptyKlass;
    emptyObjectKlass = emptyObjectKlass->AddTransaction(allocator, strs::__PROTO__);
    emptyObjectKlass = emptyObjectKlass->AddTransaction(allocator, strs::CONSTRUCTOR);
    result = emptyObjectKlass->Find(strs::__PROTO__, expectedObjectProtoOffset);
    hydra_assert(result, "__proto__ must be found");
    result = emptyObjectKlass->Find(strs::CONSTRUCTOR, expectedObjectConstructorOffset);
    hydra_assert(result, "constructor must be found");

    expectedArrayKlass = emptyObjectKlass->AddTransaction(allocator, strs::LENGTH);
    result = expectedArrayKlass->Find(strs::LENGTH, expectedArrayLengthOffset);
    hydra_assert(result, "length must be found");

    // Objects
    Object = emptyObjectKlass->NewObject<JSNativeFunction>(allocator, lib_Object);
    Object_prototype = emptyObjectKlass->NewObject<JSObject>(allocator);
    hydra_assert(emptyObjectKlass->IsBaseOf(Object_prototype->GetKlass()),
        "Object.prototype's klass should inherit from emptyObjectKlass");
    Object_prototype->SetIndex(expectedObjectProtoOffset,
        JSValue::FromObject(nullptr),
        JSObjectPropertyAttribute::IS_DATA_MASK |
        JSObjectPropertyAttribute::IS_WRITABLE_MASK);
    Object_prototype->SetIndex(expectedObjectConstructorOffset,
        JSValue::FromObject(Object),
        JSObjectPropertyAttribute::IS_DATA_MASK |
        JSObjectPropertyAttribute::IS_WRITABLE_MASK);
    Object->Set(allocator, strs::PROTOTYPE, JSValue::FromObject(Object_prototype));

    Function = emptyObjectKlass->NewObject<JSNativeFunction>(allocator, lib_Function);
    Function_prototype = emptyObjectKlass->NewObject<JSObject>(allocator);
    hydra_assert(emptyObjectKlass->IsBaseOf(Function_prototype->GetKlass()),
        "Function.prototype's klass should inherit from emptyObjectKlass");
    Function_prototype->SetIndex(expectedObjectProtoOffset,
        JSValue::FromObject(Object_prototype),
        JSObjectPropertyAttribute::IS_DATA_MASK |
        JSObjectPropertyAttribute::IS_WRITABLE_MASK);
    Function_prototype->SetIndex(expectedObjectConstructorOffset,
        JSValue::FromObject(Function),
        JSObjectPropertyAttribute::IS_DATA_MASK |
        JSObjectPropertyAttribute::IS_WRITABLE_MASK);
    Function->Set(allocator, strs::PROTOTYPE, JSValue::FromObject(Function_prototype));

    Object->Set(allocator, strs::__PROTO__, JSValue::FromObject(Function_prototype));
    Function->Set(allocator, strs::__PROTO__, JSValue::FromObject(Function_prototype));
}

JSObject *NewEmptyObjectSafe(gc::ThreadAllocator &allocator)
{
    JSObject *ret = emptyObjectKlass->NewObject<JSObject>(allocator);

    hydra_assert(emptyObjectKlass->IsBaseOf(ret->GetKlass()),
        "Newly allocated object's klass should inherit from emptyObjectKlass");

    ret->SetIndex(expectedObjectProtoOffset,
        JSValue::FromObject(Object_prototype),
        JSObjectPropertyAttribute::IS_DATA_MASK |
JSObjectPropertyAttribute::IS_WRITABLE_MASK);

return ret;
}

bool NewEmptyObject(gc::ThreadAllocator &allocator, JSValue &retVal, JSValue &error)
{
    retVal = JSValue::FromObject(NewEmptyObjectSafe(allocator));
    return true;
}

bool NewObjectSafe(gc::ThreadAllocator &allocator,
    JSFunction *constructor,
    JSArray *arguments,
    JSValue &retVal,
    JSValue &error)
{
    JSObject *ret = emptyObjectKlass->NewObject<JSObject>(allocator);
    hydra_assert(emptyObjectKlass->IsBaseOf(ret->GetKlass()),
        "Newly allocated object's klass should inherit from emptyObjectKlass");

    JSValue constructor___proto__;
    if (!ObjectGetSafeObject(allocator, constructor, strs::__PROTO__, constructor___proto__, error))
    {
        return false;
    }

    ret->SetIndex(expectedObjectProtoOffset,
        constructor___proto__,
        JSObjectPropertyAttribute::IS_DATA_MASK |
        JSObjectPropertyAttribute::IS_WRITABLE_MASK);

    return constructor->Call(allocator, JSValue::FromObject(ret), arguments, retVal, error);
}

bool NewObject(gc::ThreadAllocator &allocator, JSValue constructor, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    if (JSValue::GetType(constructor) != Type::T_OBJECT)
    {
        js_throw_error(TypeError, "Object dosen't support this action");
    }

    JSFunction *ctor = dynamic_cast<JSFunction*>(constructor.Object());

    if (ctor == nullptr)
    {
        js_throw_error(ValueError, "Object expected");
    }

    return NewObjectSafe(allocator, ctor, arguments, retVal, error);
}

JSArray *NewArrayInternal(gc::ThreadAllocator &allocator, size_t capacity)
{
    return JSArray::New(allocator, capacity);
}

bool ObjectGet(gc::ThreadAllocator &allocator, JSValue object, JSValue key, JSValue &retVal, JSValue &error)
{
    if (lib_isArraySafe(object))
    {
        JSArray *arr = dynamic_cast<JSArray*>(object.Object());

        if (JSValue::GetType(key) == Type::T_SMALL_INT)
        {
            return ObjectGetSafeArray(allocator, arr, static_cast<size_t>(key.SmallInt()), retVal, error);
        }
        else if (JSValue::GetType(key) == Type::T_STRING)
        {
            i64 integralKey;
            if (IsStringIntegral(key.String(), integralKey) && integralKey >= 0)
            {
                return ObjectGetSafeArray(allocator, arr, static_cast<size_t>(integralKey), retVal, error);
            }
            else
            {
                return ObjectGetSafeObject(allocator, arr, key.String(), retVal, error);
            }
        }
    }
    else if (JSValue::GetType(object) == Type::T_OBJECT)
    {
        JSValue stringKey;
        if (!ToString(allocator, key, stringKey, error))
        {
            return false;
        }

        return ObjectGetSafeObject(allocator, object.Object(), stringKey.String(), retVal, error);
    }
    else
    {
        hydra_trap("TODO: other data types");
    }
}

bool ObjectGetSafeObject(gc::ThreadAllocator &allocator, JSObject *object, String *key, JSValue &retVal, JSValue &error)
{
    JSObjectPropertyAttribute attribute;
    if (object->Get(key, retVal, attribute))
    {
        if (attribute.IsData())
        {
            return true;
        }
    }
}

bool ObjectGetSafeArray(gc::ThreadAllocator &allocator, JSArray *array, size_t key, JSValue &retVal, JSValue &error)
{
    hydra_trap("TODO");
    return false;
}

/*********************** lib ***********************/
static bool lib_Object(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    if (JSValue::GetType(thisArg) != Type::T_UNDEFINED)
    {
        js_return(thisArg);
    }
    js_return(JSValue::FromObject(NewEmptyObjectSafe(allocator)));
}

static bool lib_Function(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    hydra_trap("TODO");
    return false;
}

static bool lib_IsArray(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    hydra_trap("TODO");
    return false;
}

static bool lib_isArraySafe(JSValue value)
{
    if (JSValue::GetType(value) != Type::T_OBJECT)
    {
        return false;
    }

    JSObject *obj = value.Object();
    return dynamic_cast<JSArray*>(obj) != nullptr;
}

} // namespace semantic
} // namespace runtime
} // namespace hydra