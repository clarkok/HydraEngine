#include "Semantic.h"

#include "JSFunction.h"

#include "Klass.h"

#include <cmath>

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
    def(u"true", _TRUE)                 \
    def(u"false", _FALSE)               \
    def(u"null", _NULL)                 \
    def(u"undefined", UNDEFINED)        \
    def(u"toString", TO_STRING)         \
    def(u"NaN", _NAN)                   \
    def(u"Infinity", _INFINITY)         \
    def(u"-Infinity", _N_INFINITY)      \


static bool lib_Object(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error);
static bool lib_Object_prototype_hasOwnProperty(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error);
static bool lib_Function(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error);
static bool lib_Array_IsArray(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error);
static bool lib_Array_IsArraySafe(JSValue value);

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

    return_js_call(constructor, JSValue::FromObject(ret), arguments);
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
    // TODO get __proto__

    if (lib_Array_IsArraySafe(object))
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

static inline bool ObjectInvokeGetter(gc::ThreadAllocator &allocator, JSValue desc, JSObject *object, JSValue &retVal, JSValue &error)
{
    hydra_assert(JSValue::GetType(desc) == Type::T_OBJECT,
        "Value must be a T_OBJECT");

    JSObject *describer = desc.Object();
    JSValue getter;

    hydra_assert(describer != nullptr,
        "describer must not be null");

    if (!ObjectGetSafeObject(allocator, describer, strs::GET, getter, error))
    {
        // return undefined
        retVal = JSValue();
        return true;
    }

    hydra_assert(JSValue::GetType(getter) == Type::T_OBJECT,
        "getter must be a T_OBJECT");

    JSFunction *getterFunc = dynamic_cast<JSFunction*>(getter.Object());

    hydra_assert(getterFunc != nullptr,
        "getterFunc must not be null");

    return_js_call(getterFunc, JSValue::FromObject(object), nullptr);
}

bool ObjectGetSafeObject(gc::ThreadAllocator &allocator, JSObject *object, String *key, JSValue &retVal, JSValue &error)
{
    JSObjectPropertyAttribute attribute;
    if (!object->Get(key, retVal, attribute))
    {
        retVal = JSValue();
        return true;
    }

    if (attribute.IsData())
    {
        return true;
    }

    return ObjectInvokeGetter(allocator, retVal, object, retVal, error);
}

bool ObjectGetSafeArray(gc::ThreadAllocator &allocator, JSArray *array, size_t key, JSValue &retVal, JSValue &error)
{
    JSObjectPropertyAttribute attribute;
    if (!array->Get(key, retVal, attribute))
    {
        retVal = JSValue();
        return true;
    }

    if (attribute.IsData())
    {
        return true;
    }

    return ObjectInvokeGetter(allocator, retVal, array, retVal, error);
}

bool ObjectSet(gc::ThreadAllocator &allocator, JSValue object, JSValue key, JSValue value, JSValue &error)
{
    if (lib_Array_IsArraySafe(object))
    {
        JSArray *arr = dynamic_cast<JSArray*>(object.Object());

        if (JSValue::GetType(key) == Type::T_SMALL_INT)
        {
            return ObjectSetSafeArray(allocator, arr, static_cast<size_t>(key.SmallInt()), value, error);
        }
        else if (JSValue::GetType(key) == Type::T_STRING)
        {
            i64 integralKey;
            if (IsStringIntegral(key.String(), integralKey) && integralKey >= 0)
            {
                return ObjectSetSafeArray(allocator, arr, static_cast<size_t>(integralKey), value, error);
            }
            else
            {
                return ObjectSetSafeObject(allocator, arr, key.String(), value, error);
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

        return ObjectSetSafeObject(allocator, object.Object(), stringKey.String(), value, error);
    }
    else
    {
        hydra_trap("TODO: other data types");
    }
}

static inline bool ObjectInvokeSetter(gc::ThreadAllocator &allocator, JSValue desc, JSObject *object, JSValue value, JSValue &error)
{
    hydra_assert(JSValue::GetType(desc) == Type::T_OBJECT,
        "Value must be a T_OBJECT");

    JSObject *describer = desc.Object();
    JSValue setter;

    hydra_assert(describer != nullptr,
        "describer must not be null");

    if (!ObjectGetSafeObject(allocator, describer, strs::SET, setter, error))
    {
        js_throw_error(TypeError, "No setter defined");
    }

    hydra_assert(JSValue::GetType(setter) == Type::T_OBJECT,
        "getter must be a T_OBJECT");

    JSFunction *setterFunc = dynamic_cast<JSFunction*>(setter.Object());

    hydra_assert(setterFunc != nullptr,
        "getterFunc must not be null");

    JSValue retVal; // that we don't care

    JSArray *arguments = NewArrayInternal(allocator, 1);
    arguments->Set(allocator, 0, value);

    return_js_call(setterFunc, JSValue::FromObject(object), arguments);
}

bool ObjectSetSafeObject(gc::ThreadAllocator &allocator, JSObject *object, String *key, JSValue value, JSValue &error)
{
    JSObjectPropertyAttribute attribute;
    JSValue retVal;
    if (!object->Get(key, retVal, attribute))
    {
        object->Set(allocator, key, value);
        return true;
    }

    if (!attribute.IsWritable())
    {
        js_throw_error(TypeError, "Write to a readonly property");
    }

    if (attribute.IsData())
    {
        object->Set(allocator, key, value, attribute);
        return true;
    }

    return ObjectInvokeSetter(allocator, retVal, object, value, error);
}

bool ObjectSetSafeArray(gc::ThreadAllocator &allocator, JSArray *array, size_t key, JSValue value, JSValue &error)
{
    JSObjectPropertyAttribute attribute;
    JSValue retVal;
    if (!array->Get(key, retVal, attribute))
    {
        array->Set(allocator, key, value);
        return true;
    }

    if (!attribute.IsWritable())
    {
        js_throw_error(TypeError, "Write to a readonly property");
    }

    if (attribute.IsData())
    {
        array->Set(allocator, key, value, attribute);
        return true;
    }

    return ObjectInvokeSetter(allocator, retVal, array, value, error);
}

bool ObjectDelete(gc::ThreadAllocator &allocator, JSValue object, JSValue key, JSValue &error)
{
    JSArray *arguments = NewArrayInternal(allocator, 1);
    auto result = ObjectSetSafeArray(allocator, arguments, 0, key, error);
    if (!result)
    {
        js_throw(error);
    }

    JSValue hasOwnProperty;
    lib_call(hasOwnProperty, lib_Object_prototype_hasOwnProperty, object, arguments);

    if (!hasOwnProperty.Boolean())
    {
        return true;
    }

    return ObjectSet(allocator, object, key, JSValue(), error);
}

/*********************** String ***********************/

bool IsStringIntegral(String *str, i64 &value)
{
    value = 0;
    size_t length = str->length();

    for (size_t i = 0; i < length; ++i)
    {
        auto ch = str->at(i);
        if (ch >= '0' && ch <= '9')
        {
            value *= 10;
            value += ch - '0';
        }
        else
        {
            return false;
        }
    }

    return true;
}

static inline String *NumberToString(gc::ThreadAllocator &allocator, double value)
{
    // TODO use other algorithm
    if (std::isnan(value))
    {
        return strs::_NAN;
    }

    if (std::isinf(value))
    {
        if (value > 0)
        {
            return strs::_INFINITY;
        }
        else
        {
            return strs::_N_INFINITY;
        }
    }

    std::string str = std::to_string(value);

    return String::New(allocator, str.begin(), str.end());
}

static inline String *SmallIntToString(gc::ThreadAllocator &allocator, i64 value)
{
}

bool ToString(gc::ThreadAllocator &allocator, JSValue value, JSValue &retVal, JSValue &error)
{
    switch (JSValue::GetType(value))
    {
    case Type::T_BOOLEAN:
        js_return(JSValue::FromString(value.Boolean() ? strs::_TRUE : strs::_FALSE));
        break;
    case Type::T_NUMBER:
        js_return(JSValue::FromString(NumberToString(allocator, value.Number())));
        break;
    case Type::T_OBJECT:
    {
        JSObject *object = value.Object();
        if (!object)
        {
            js_return(JSValue::FromString(strs::_NULL));
        }
        JSValue toString;
        auto result = ObjectGetSafeObject(allocator, object, strs::TO_STRING, toString, error);
        if (!result)
        {
            js_throw(error);
        }
        hydra_assert(JSValue::GetType(toString) == Type::T_OBJECT,
            "toString must be a T_OBJECT");

        JSFunction *toStringFunc = dynamic_cast<JSFunction *>(toString.Object());
        hydra_assert(toStringFunc, "toStringFunc must not be null");

        return_js_call(toStringFunc, value, nullptr);
        break;
    }
    case Type::T_SMALL_INT:
        break;
    case Type::T_STRING:
        js_return(JSValue::FromString(value.String()));
        break;
    case Type::T_UNDEFINED:
    case Type::T_NOT_EXISTS:
        js_return(JSValue::FromString(strs::UNDEFINED));
        break;
    default:
        hydra_trap("Unknown type");
    }
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

static bool lib_Object_prototype_hasOwnProperty(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    if (JSValue::GetType(thisArg) != Type::T_OBJECT)
    {
        js_throw_error(TypeError, "this is not an object");
    }

    JSValue keyArg;
    auto result = ObjectGetSafeArray(allocator, arguments, 0, keyArg, error);
    if (!result)
    {
        js_throw(error);
    }

    if (lib_Array_IsArraySafe(thisArg))
    {
        JSArray *array = dynamic_cast<JSArray*>(thisArg.Object());
        hydra_assert(array, "array must not be null");
        if (JSValue::GetType(keyArg) == Type::T_SMALL_INT)
        {
            JSValue value;
            JSObjectPropertyAttribute attribute;
            result = array->Get(keyArg.SmallInt(), value, attribute);
            if (!result)
            {
                js_return(JSValue::FromBoolean(false));
            }

            js_return(JSValue::FromBoolean(value != JSValue() && attribute.HasValue()));
        }
        else if (JSValue::GetType(keyArg) == Type::T_STRING)
        {
            i64 integralKey;
            if (IsStringIntegral(keyArg.String(), integralKey) && integralKey >= 0)
            {
                JSValue value;
                JSObjectPropertyAttribute attribute;
                result = array->Get(static_cast<size_t>(integralKey), value, attribute);
                if (!result)
                {
                    js_return(JSValue::FromBoolean(false));
                }

                js_return(JSValue::FromBoolean(value != JSValue() && attribute.HasValue()));
            }
        }
    }

    JSValue keyString;
    result = ToString(allocator, keyArg, keyString, error);
    if (!result)
    {
        js_throw(error);
    }

    String *key = keyString.String();
    hydra_assert(key, "key must not be null");

    JSObject *obj = thisArg.Object();
    JSValue value;
    JSObjectPropertyAttribute attribute;
    result = obj->Get(key, value, attribute);

    if (!result)
    {
        js_return(JSValue::FromBoolean(false));
    }
    js_return(JSValue::FromBoolean(value != JSValue() && attribute.HasValue()));
}

static bool lib_Function(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    hydra_trap("TODO");
    return false;
}

static bool lib_Array_IsArray(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    hydra_trap("TODO");
    return false;
}

static bool lib_Array_IsArraySafe(JSValue value)
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