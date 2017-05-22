#include "Semantic.h"

#include "JSFunction.h"

#include "Klass.h"

#include "VirtualMachine/Scope.h"
#include "VirtualMachine/IRInsts.h"
#include "VirtualMachine/CompiledFunction.h"

#include <cmath>
#include <iostream>

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
    def(u"global", GLOBAL)              \
    def(u"Object", OBJECT)              \
    def(u"Array", ARRAY)                \
    def(u"Function", FUNCTION)          \
    def(u"boolean", _BOOLEAN)           \
    def(u"number", NUMBER)              \
    def(u"symbol", SYMBOL)              \
    def(u"string", STRING)              \
    def(u"object", _OBJECT)             \
    def(u"__write", __WRITE)            \


static bool lib_Object(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error);
static bool lib_Object_prototype_hasOwnProperty(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error);
static bool lib_Function(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error);
static bool lib_Array(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error);
static bool lib_Array_IsArray(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error);
static bool lib_Array_IsArraySafe(JSValue value);
static void InitializeArray(gc::ThreadAllocator &allocator);

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
static JSObject *Array_prototype;
static JSObject *Global;

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
    scan(Array);
    scan(Array_prototype);

    scan(Global);
}

void Initialize(gc::ThreadAllocator &allocator)
{
    gc::Heap::GetInstance()->RegisterRootScanFunc(RootScan);

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

    JSValue error;
    Global = NewEmptyObjectSafe(allocator);

    result = ObjectSetSafeObject(allocator, Global, strs::GLOBAL, JSValue::FromObject(Global), error);
    hydra_assert(result, "Error on setting global.global");

    result = ObjectSetSafeObject(allocator, Global, strs::OBJECT, JSValue::FromObject(Object), error);
    hydra_assert(result, "Error on setting global.Object");

    result = ObjectSetSafeObject(allocator, Global, strs::FUNCTION, JSValue::FromObject(Function), error);
    hydra_assert(result, "Error on setting global.Function");

    // Array
    Array = NewNativeFunc(allocator, lib_Array);
    Array_prototype = NewEmptyObjectSafe(allocator);
    result = ObjectSetSafeObject(allocator, Array, strs::PROTOTYPE, JSValue::FromObject(Array_prototype), error);
    hydra_assert(result, "Error on setting Array.prototype");

    result = ObjectSetSafeObject(allocator, Global, strs::ARRAY, JSValue::FromObject(Array), error);
    hydra_assert(result, "Error on setting global.Array");
    InitializeArray(allocator);

    auto __write = NewNativeFunc(allocator, [](gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error) -> bool
    {
        for (size_t i = 0; i < arguments->GetLength(); ++i)
        {
            JSValue retVal;
            JSValue error;
            JSObjectPropertyAttribute attribute;

            if (!arguments->Get(i, retVal, attribute))
            {
                js_throw_error(ValueError, "Unable to get arguments");
            }

            if (!ToString(allocator, retVal, retVal, error))
            {
                return false;
            }

            String::Print(retVal.String());
        }
        String::Println(String::Empty(allocator));
    });

    result = ObjectSetSafeObject(allocator, Global, strs::__WRITE, JSValue::FromObject(__write), error);
    hydra_assert(result, "Error on setting global.__write");
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

bool NewObjectWithInst(gc::ThreadAllocator &allocator, vm::Scope *scope, vm::IRInst *inst, JSValue &retVal, JSValue &error)
{
    retVal = JSValue::FromObject(NewEmptyObjectSafe(allocator));

    for (auto &pair : inst->As<vm::ir::Object>()->Initialization)
    {
        JSValue key = scope->GetRegs()->at(pair.first->Index);
        JSValue value = scope->GetRegs()->at(pair.second->Index);

        if (!ObjectSet(allocator, retVal, key, value, error))
        {
            return false;
        }
    }

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

bool Call(gc::ThreadAllocator &allocator, JSValue callee, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    if (JSValue::GetType(callee) != Type::T_OBJECT)
    {
        js_throw_error(TypeError, "Object dosen't support this action");
    }

    JSFunction *func = dynamic_cast<JSFunction*>(callee.Object());

    if (func == nullptr)
    {
        js_throw_error(ValueError, "Object expected");
    }

    return func->Call(allocator, thisArg, arguments, retVal, error);
}

bool GetGlobal(gc::ThreadAllocator &allocator, String *name, JSValue &retVal, JSValue &error)
{
    hydra_assert(name, "name should not be nullptr");
    return ObjectGetSafeObject(allocator, Global, name, retVal, error);
}

bool SetGlobal(gc::ThreadAllocator &allocator, String *name, JSValue value, JSValue &error)
{
    hydra_assert(name, "name should not be nullptr");
    return ObjectSetSafeObject(allocator, Global, name, value, error);
}

JSArray *NewArrayInternal(gc::ThreadAllocator &allocator, size_t capacity)
{
    auto ret = JSArray::New(allocator, capacity);
    ret->SetIndex(expectedObjectProtoOffset, JSValue::FromObject(Array_prototype));
    return ret;
}

bool NewArrayWithInst(gc::ThreadAllocator &allocator, vm::Scope *scope, vm::IRInst *inst, JSValue &retVal, JSValue &error)
{
    auto ret = NewArrayInternal(allocator, inst->As<vm::ir::Array>()->Initialization.size());
    retVal = JSValue::FromObject(ret);

    size_t index = 0;
    for (auto &ref : inst->As<vm::ir::Array>()->Initialization)
    {
        if (!ObjectSetSafeArray(allocator, ret, index++, scope->GetRegs()->at(ref->Index), error))
        {
            return false;
        }
    }
    return true;
}

static void InitializeFunction(gc::ThreadAllocator &allocator, JSFunction *func)
{
    func->SetIndex(expectedObjectProtoOffset,
        JSValue::FromObject(Function_prototype),
        JSObjectPropertyAttribute::IS_DATA_MASK |
        JSObjectPropertyAttribute::IS_WRITABLE_MASK);
}

JSCompiledFunction *NewRootFunc(gc::ThreadAllocator &allocator, vm::IRFunc *func)
{
    auto ret = emptyObjectKlass->NewObject<JSCompiledFunction>(allocator, nullptr, nullptr, func);
    InitializeFunction(allocator, ret);
    return ret;
}

JSNativeFunction *NewNativeFunc(gc::ThreadAllocator &allocator, JSNativeFunction::Functor func)
{
    auto ret = emptyObjectKlass->NewObject<JSNativeFunction>(allocator, func);
    InitializeFunction(allocator, ret);
    return ret;
}

bool NewFuncWithInst(gc::ThreadAllocator &allocator, vm::Scope *scope, vm::IRInst *inst, JSValue &retVal, JSValue &error)
{
    auto funcInst = inst->As<vm::ir::Func>();

    auto captured = Array::New(allocator, funcInst->Captured.size());
    size_t index = 0;
    for (auto &ref : funcInst->Captured)
    {
        captured->at(index++) = JSValue::FromSmallInt(ref->Index);
    }

    hydra_assert(funcInst->FuncPtr != nullptr,
        "funcInst->FuncPtr cannot be null");

    auto ret = emptyObjectKlass->NewObject<JSCompiledFunction>(allocator, scope, captured, funcInst->FuncPtr);
    InitializeFunction(allocator, ret);
    retVal = JSValue::FromObject(ret);

    return true;
}

bool NewArrowWithInst(gc::ThreadAllocator &allocator, vm::Scope *scope, vm::IRInst *inst, JSValue &retVal, JSValue &error)
{
    auto funcInst = inst->As<vm::ir::Func>();

    auto captured = Array::New(allocator, funcInst->Captured.size());
    size_t index = 0;
    for (auto &ref : funcInst->Captured)
    {
        captured->at(index++) = JSValue::FromSmallInt(ref->Index);
    }

    hydra_assert(funcInst->FuncPtr != nullptr,
        "funcInst->FuncPtr cannot be null");

    auto ret = emptyObjectKlass->NewObject<JSCompiledArrowFunction>(allocator, scope, captured, funcInst->FuncPtr);
    InitializeFunction(allocator, ret);
    retVal = JSValue::FromObject(ret);

    return true;
}

bool ObjectGet(gc::ThreadAllocator &allocator, JSValue object, JSValue key, JSValue &retVal, JSValue &error)
{
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

    if (JSValue::GetType(object) == Type::T_OBJECT)
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
        return false;
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

bool ObjectGetSafeObjectInternal(
    gc::ThreadAllocator &allocator,
    JSObject *object,
    String *key,
    JSValue &retVal,
    JSObjectPropertyAttribute &attribute,
    JSValue &error)
{
    if (!object->Get(key, retVal, attribute))
    {
        JSValue __proto__;
        JSObjectPropertyAttribute __proto__attribute;
        object->GetIndex(expectedObjectProtoOffset, __proto__, __proto__attribute);

        hydra_assert(JSValue::GetType(__proto__) == Type::T_OBJECT,
            "__proto__ must be an object");

        JSObject *proto = __proto__.Object();

        if (!proto)
        {
            retVal = JSValue();
            return true;
        }

        return ObjectGetSafeObjectInternal(
            allocator,
            proto,
            key,
            retVal,
            attribute,
            error);
    }
}

bool ObjectGetSafeObject(gc::ThreadAllocator &allocator, JSObject *object, String *key, JSValue &retVal, JSValue &error)
{
    JSObjectPropertyAttribute attribute;
    if (!object->Get(key, retVal, attribute))
    {
        JSValue __proto__;
        JSObjectPropertyAttribute __proto__attribute;
        object->GetIndex(expectedObjectProtoOffset, __proto__, __proto__attribute);

        hydra_assert(JSValue::GetType(__proto__) == Type::T_OBJECT,
            "__proto__ must be an object");

        JSObject *proto = __proto__.Object();

        if (!proto)
        {
            retVal = JSValue();
            return true;
        }

        if (!ObjectGetSafeObjectInternal(allocator, proto, key, retVal, attribute, error))
        {
            return false;
        }
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

    if (JSValue::GetType(object) == Type::T_OBJECT)
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
        return false;
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
        js_return(JSValue::FromString(NumberToString(allocator, static_cast<double>(value.SmallInt()))));
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
        return false;
    }
}

/******************************** Operation *****************************/
bool OpAdd(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if (typeA == Type::T_SMALL_INT && typeB == Type::T_SMALL_INT)
    {
        js_return(JSValue::FromSmallInt(a.SmallInt() + b.SmallInt()));
    }

    if ((typeA == Type::T_SMALL_INT || typeA == Type::T_NUMBER) &&
        (typeB == Type::T_SMALL_INT || typeB == Type::T_NUMBER))
    {
        double numberA = (typeA == Type::T_SMALL_INT) ? a.SmallInt() : a.Number();
        double numberB = (typeB == Type::T_SMALL_INT) ? b.SmallInt() : b.Number();

        js_return(JSValue::FromNumber(numberA + numberB));
    }

    if (typeA == Type::T_STRING && typeB == Type::T_STRING)
    {
        String *concated = String::Concat(allocator, a.String(), b.String());
        js_return(JSValue::FromString(concated));
    }

    if (typeA == Type::T_UNDEFINED || typeA == Type::T_NOT_EXISTS ||
        typeB == Type::T_UNDEFINED || typeB == Type::T_NOT_EXISTS)
    {
        js_return(JSValue::FromNumber(NAN));
    }

    if (!ToString(allocator, a, a, error))
    {
        return false;
    }
    if (!ToString(allocator, b, b, error))
    {
        return false;
    }
    String *concated = String::Concat(allocator, a.String(), b.String());
    js_return(JSValue::FromString(concated));
}

bool OpSub(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if (typeA == Type::T_SMALL_INT && typeB == Type::T_SMALL_INT)
    {
        js_return(JSValue::FromSmallInt(a.SmallInt() - b.SmallInt()));
    }

    if ((typeA == Type::T_SMALL_INT || typeA == Type::T_NUMBER) &&
        (typeB == Type::T_SMALL_INT || typeB == Type::T_NUMBER))
    {
        double numberA = (typeA == Type::T_SMALL_INT) ? a.SmallInt() : a.Number();
        double numberB = (typeB == Type::T_SMALL_INT) ? b.SmallInt() : b.Number();

        js_return(JSValue::FromNumber(numberA - numberB));
    }

    js_return(JSValue::FromNumber(NAN));
}

bool OpMul(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if (typeA == Type::T_SMALL_INT && typeB == Type::T_SMALL_INT)
    {
        js_return(JSValue::FromSmallInt(a.SmallInt() * b.SmallInt()));
    }

    if ((typeA == Type::T_SMALL_INT || typeA == Type::T_NUMBER) &&
        (typeB == Type::T_SMALL_INT || typeB == Type::T_NUMBER))
    {
        double numberA = (typeA == Type::T_SMALL_INT) ? a.SmallInt() : a.Number();
        double numberB = (typeB == Type::T_SMALL_INT) ? b.SmallInt() : b.Number();

        js_return(JSValue::FromNumber(numberA * numberB));
    }

    js_return(JSValue::FromNumber(NAN));
}

bool OpDiv(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if ((typeA == Type::T_SMALL_INT || typeA == Type::T_NUMBER) &&
        (typeB == Type::T_SMALL_INT || typeB == Type::T_NUMBER))
    {
        double numberA = (typeA == Type::T_SMALL_INT) ? a.SmallInt() : a.Number();
        double numberB = (typeB == Type::T_SMALL_INT) ? b.SmallInt() : b.Number();

        js_return(JSValue::FromNumber(numberA / numberB));
    }

    js_return(JSValue::FromNumber(NAN));
}

bool OpMod(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if (typeA == Type::T_SMALL_INT && typeB == Type::T_SMALL_INT)
    {
        js_return(JSValue::FromSmallInt(a.SmallInt() % b.SmallInt()));
    }

    if ((typeA == Type::T_SMALL_INT || typeA == Type::T_NUMBER) &&
        (typeB == Type::T_SMALL_INT || typeB == Type::T_NUMBER))
    {
        double numberA = (typeA == Type::T_SMALL_INT) ? a.SmallInt() : a.Number();
        double numberB = (typeB == Type::T_SMALL_INT) ? b.SmallInt() : b.Number();

        js_return(JSValue::FromNumber(std::fmod(numberA, numberB)));
    }

    js_return(JSValue::FromNumber(NAN));
}

bool OpBand(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if (typeA == Type::T_SMALL_INT && typeB == Type::T_SMALL_INT)
    {
        js_return(JSValue::FromSmallInt(a.SmallInt() & b.SmallInt()));
    }

    if ((typeA == Type::T_SMALL_INT || typeA == Type::T_NUMBER) &&
        (typeB == Type::T_SMALL_INT || typeB == Type::T_NUMBER))
    {
        u64 numberA = (typeA == Type::T_SMALL_INT) ? a.SmallInt() : static_cast<u64>(a.Number());
        u64 numberB = (typeB == Type::T_SMALL_INT) ? b.SmallInt() : static_cast<u64>(b.Number());

        js_return(JSValue::FromSmallInt(numberA & numberB));
    }

    js_return(JSValue::FromNumber(0));
}

bool OpBor(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    u64 numberA = 0, numberB = 0;

    if (typeA == Type::T_SMALL_INT)
    {
        numberA = a.SmallInt();
    }
    else if (typeA == Type::T_NUMBER)
    {
        numberA = static_cast<u64>(a.Number());
    }

    if (typeB == Type::T_SMALL_INT)
    {
        numberB = b.SmallInt();
    }
    else if (typeB == Type::T_NUMBER)
    {
        numberB = static_cast<u64>(b.Number());
    }

    js_return(JSValue::FromSmallInt(numberA | numberB));
}

bool OpBnot(gc::ThreadAllocator &allocator, JSValue a, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);

    u64 numberA = 0;

    if (typeA == Type::T_SMALL_INT)
    {
        numberA = a.SmallInt();
    }
    else if (typeA == Type::T_NUMBER)
    {
        numberA = static_cast<u64>(a.Number());
    }

    js_return(JSValue::FromSmallInt(~numberA));
}

bool OpLnot(gc::ThreadAllocator &allocator, JSValue a, JSValue &retVal, JSValue &error)
{
    js_return(JSValue::FromBoolean(!ToBoolean(a)));
}

bool OpSll(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    u64 numberA = 0, numberB = 0;

    if (typeA == Type::T_SMALL_INT)
    {
        numberA = a.SmallInt();
    }
    else if (typeA == Type::T_NUMBER)
    {
        numberA = static_cast<u64>(a.Number());
    }

    if (typeB == Type::T_SMALL_INT)
    {
        numberB = b.SmallInt();
    }
    else if (typeB == Type::T_NUMBER)
    {
        numberB = static_cast<u64>(b.Number());
    }

    js_return(JSValue::FromSmallInt(numberA << numberB));
}

bool OpSrl(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    i64 numberA = 0, numberB = 0;

    if (typeA == Type::T_SMALL_INT)
    {
        numberA = a.SmallInt();
    }
    else if (typeA == Type::T_NUMBER)
    {
        numberA = static_cast<u64>(a.Number());
    }

    if (typeB == Type::T_SMALL_INT)
    {
        numberB = b.SmallInt();
    }
    else if (typeB == Type::T_NUMBER)
    {
        numberB = static_cast<u64>(b.Number());
    }

    js_return(JSValue::FromSmallInt(numberA << numberB));
}

bool OpSrr(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    i64 numberA = 0, numberB = 0;

    if (typeA == Type::T_SMALL_INT)
    {
        numberA = a.SmallInt();
    }
    else if (typeA == Type::T_NUMBER)
    {
        numberA = static_cast<u64>(a.Number());
    }

    if (typeB == Type::T_SMALL_INT)
    {
        numberB = b.SmallInt();
    }
    else if (typeB == Type::T_NUMBER)
    {
        numberB = static_cast<u64>(b.Number());
    }

    js_return(JSValue::FromSmallInt(numberA << numberB));
}

bool OpEq(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if (typeA == Type::T_SMALL_INT && typeB == Type::T_SMALL_INT)
    {
        js_return(JSValue::FromBoolean(a.SmallInt() == b.SmallInt()));
    }

    if ((typeA == Type::T_SMALL_INT || typeA == Type::T_NUMBER) &&
        (typeB == Type::T_SMALL_INT || typeB == Type::T_NUMBER))
    {
        double numberA = (typeA == Type::T_SMALL_INT) ? a.SmallInt() : a.Number();
        double numberB = (typeB == Type::T_SMALL_INT) ? b.SmallInt() : b.Number();

        js_return(JSValue::FromBoolean(numberA == numberB));
    }

    if ((typeA == Type::T_UNDEFINED || typeA == Type::T_NOT_EXISTS) &&
        (typeB == Type::T_UNDEFINED || typeB == Type::T_NOT_EXISTS))
    {
        js_return(JSValue::FromBoolean(true));
    }

    if (!ToString(allocator, a, a, error))
    {
        return false;
    }
    if (!ToString(allocator, b, b, error))
    {
        return false;
    }

    js_return(JSValue::FromBoolean(a.String()->EqualsTo(b.String())));
}

bool OpEqq(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if ((typeA == Type::T_UNDEFINED || typeA == Type::T_NOT_EXISTS) &&
        (typeB == Type::T_UNDEFINED || typeB == Type::T_NOT_EXISTS))
    {
        js_return(JSValue::FromBoolean(true));
    }

    if (typeA != typeB)
    {
        js_return(JSValue::FromBoolean(false));
    }

    if (typeA == Type::T_STRING)
    {
        js_return(JSValue::FromBoolean(a.String()->EqualsTo(b.String())));
    }

    js_return(JSValue::FromBoolean(a.Payload == b.Payload));
}

bool OpNe(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    JSValue res;
    if (!OpEq(allocator, a, b, res, error))
    {
        return false;
    }

    js_return(JSValue::FromBoolean(!res.Boolean()));
}

bool OpNee(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    JSValue res;
    if (!OpEqq(allocator, a, b, res, error))
    {
        return false;
    }
    js_return(JSValue::FromBoolean(!res.Boolean()));
}

bool OpLt(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if (typeA == Type::T_SMALL_INT && typeB == Type::T_SMALL_INT)
    {
        js_return(JSValue::FromBoolean(a.SmallInt() < b.SmallInt()));
    }

    if ((typeA == Type::T_SMALL_INT || typeA == Type::T_NUMBER) &&
        (typeB == Type::T_SMALL_INT || typeB == Type::T_NUMBER))
    {
        double numberA = (typeA == Type::T_SMALL_INT) ? a.SmallInt() : a.Number();
        double numberB = (typeB == Type::T_SMALL_INT) ? b.SmallInt() : b.Number();

        js_return(JSValue::FromBoolean(numberA < numberB));
    }

    if ((typeA == Type::T_UNDEFINED || typeA == Type::T_NOT_EXISTS) &&
        (typeB == Type::T_UNDEFINED || typeB == Type::T_NOT_EXISTS))
    {
        js_return(JSValue::FromBoolean(false));
    }

    if (!ToString(allocator, a, a, error))
    {
        return false;
    }
    if (!ToString(allocator, b, b, error))
    {
        return false;
    }

    js_return(JSValue::FromBoolean(a.String()->Compare(b.String()) < 0));
}

bool OpLe(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if (typeA == Type::T_SMALL_INT && typeB == Type::T_SMALL_INT)
    {
        js_return(JSValue::FromBoolean(a.SmallInt() <= b.SmallInt()));
    }

    if ((typeA == Type::T_SMALL_INT || typeA == Type::T_NUMBER) &&
        (typeB == Type::T_SMALL_INT || typeB == Type::T_NUMBER))
    {
        double numberA = (typeA == Type::T_SMALL_INT) ? a.SmallInt() : a.Number();
        double numberB = (typeB == Type::T_SMALL_INT) ? b.SmallInt() : b.Number();

        js_return(JSValue::FromBoolean(numberA <= numberB));
    }

    if ((typeA == Type::T_UNDEFINED || typeA == Type::T_NOT_EXISTS) &&
        (typeB == Type::T_UNDEFINED || typeB == Type::T_NOT_EXISTS))
    {
        js_return(JSValue::FromBoolean(true));
    }

    if (!ToString(allocator, a, a, error))
    {
        return false;
    }
    if (!ToString(allocator, b, b, error))
    {
        return false;
    }

    js_return(JSValue::FromBoolean(a.String()->Compare(b.String()) <= 0));
}

bool OpGt(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if (typeA == Type::T_SMALL_INT && typeB == Type::T_SMALL_INT)
    {
        js_return(JSValue::FromBoolean(a.SmallInt() > b.SmallInt()));
    }

    if ((typeA == Type::T_SMALL_INT || typeA == Type::T_NUMBER) &&
        (typeB == Type::T_SMALL_INT || typeB == Type::T_NUMBER))
    {
        double numberA = (typeA == Type::T_SMALL_INT) ? a.SmallInt() : a.Number();
        double numberB = (typeB == Type::T_SMALL_INT) ? b.SmallInt() : b.Number();

        js_return(JSValue::FromBoolean(numberA > numberB));
    }

    if ((typeA == Type::T_UNDEFINED || typeA == Type::T_NOT_EXISTS) &&
        (typeB == Type::T_UNDEFINED || typeB == Type::T_NOT_EXISTS))
    {
        js_return(JSValue::FromBoolean(true));
    }

    if (!ToString(allocator, a, a, error))
    {
        return false;
    }
    if (!ToString(allocator, b, b, error))
    {
        return false;
    }

    js_return(JSValue::FromBoolean(a.String()->Compare(b.String()) > 0));
}

bool OpGe(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if (typeA == Type::T_SMALL_INT && typeB == Type::T_SMALL_INT)
    {
        js_return(JSValue::FromBoolean(a.SmallInt() >= b.SmallInt()));
    }

    if ((typeA == Type::T_SMALL_INT || typeA == Type::T_NUMBER) &&
        (typeB == Type::T_SMALL_INT || typeB == Type::T_NUMBER))
    {
        double numberA = (typeA == Type::T_SMALL_INT) ? a.SmallInt() : a.Number();
        double numberB = (typeB == Type::T_SMALL_INT) ? b.SmallInt() : b.Number();

        js_return(JSValue::FromBoolean(numberA >= numberB));
    }

    if ((typeA == Type::T_UNDEFINED || typeA == Type::T_NOT_EXISTS) &&
        (typeB == Type::T_UNDEFINED || typeB == Type::T_NOT_EXISTS))
    {
        js_return(JSValue::FromBoolean(true));
    }

    if (!ToString(allocator, a, a, error))
    {
        return false;
    }
    if (!ToString(allocator, b, b, error))
    {
        return false;
    }

    js_return(JSValue::FromBoolean(a.String()->Compare(b.String()) >= 0));
}

bool OpIn(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if (typeB != Type::T_OBJECT)
    {
        js_throw_error(TypeError, "Right hand is not an object");
    }

    JSObject *object = b.Object();
    JSValue key;

    if (!ToString(allocator, a, key, error))
    {
        return false;
    }

    JSValue value;
    JSObjectPropertyAttribute attribute;
    if (!ObjectGetSafeObject(allocator, object, key.String(), value, error))
    {
        return false;
    }

    js_return(JSValue::FromBoolean(value != JSValue()));
}

bool OpInstanceOf(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);
    auto typeB = JSValue::GetType(b);

    if (typeA != Type::T_OBJECT)
    {
        js_throw_error(TypeError, "Left hand is not an object");
    }

    if (typeB != Type::T_OBJECT)
    {
        js_throw_error(TypeError, "Right hand is not an object");
    }

    JSObject *left = a.Object();
    JSObject *right = b.Object();
    JSValue rightPrototype;

    if (!ObjectGetSafeObject(allocator, right, strs::PROTOTYPE, rightPrototype, error))
    {
        return false;
    }

    if (JSValue::GetType(rightPrototype) != Type::T_OBJECT || rightPrototype.Object() == nullptr)
    {
        js_return(JSValue::FromBoolean(false));
    }

    JSValue leftProto = JSValue::FromObject(left);
    while (JSValue::GetType(leftProto) == Type::T_OBJECT && leftProto.Object() != nullptr);
    {
        if (!ObjectGetSafeObject(allocator, leftProto.Object(), strs::__PROTO__, leftProto, error))
        {
            return false;
        }
        if (leftProto == rightPrototype)
        {
            js_return(JSValue::FromBoolean(true));
        }
    }

    js_return(JSValue::FromBoolean(false));
}

bool OpTypeOf(gc::ThreadAllocator &allocator, JSValue a, JSValue &retVal, JSValue &error)
{
    auto typeA = JSValue::GetType(a);

    switch (typeA)
    {
    case hydra::runtime::Type::T_UNDEFINED:
        js_return(JSValue::FromString(strs::UNDEFINED));
        break;
    case hydra::runtime::Type::T_NOT_EXISTS:
        js_return(JSValue::FromString(strs::UNDEFINED));
        break;
    case hydra::runtime::Type::T_BOOLEAN:
        js_return(JSValue::FromString(strs::_BOOLEAN));
        break;
    case hydra::runtime::Type::T_SMALL_INT:
        js_return(JSValue::FromString(strs::NUMBER));
        break;
    case hydra::runtime::Type::T_NUMBER:
        js_return(JSValue::FromString(strs::NUMBER));
        break;
    case hydra::runtime::Type::T_SYMBOL:
        js_return(JSValue::FromString(strs::SYMBOL));
        break;
    case hydra::runtime::Type::T_STRING:
        js_return(JSValue::FromString(strs::STRING));
        break;
    case hydra::runtime::Type::T_OBJECT:
        js_return(JSValue::FromString(strs::_OBJECT));
        break;
    default:
        hydra_trap("Error typeof value");
        return false;
        break;
    }
}

vm::Scope *NewScope(gc::ThreadAllocator &allocator, vm::Scope *upper, vm::IRInst *inst)
{
    runtime::Array *table = Array::New(allocator, inst->As<vm::ir::PushScope>()->Size);
    runtime::Array *captured = Array::New(allocator, inst->As<vm::ir::PushScope>()->Captured.size());

    size_t index = 0;
    for (auto &ref : inst->As<vm::ir::PushScope>()->Captured)
    {
        captured->at(index++) = upper->GetRegs()->at(ref->Index);
    }

    vm::Scope *ret = allocator.AllocateAuto<vm::Scope>(upper,
        upper->GetRegs(),
        table,
        captured,
        upper->GetThisArg(),
        upper->GetArguments());

    return ret;
}

bool ToBoolean(JSValue a)
{
    auto typeA = JSValue::GetType(a);

    bool boolA = false;

    switch (typeA)
    {
        case Type::T_BOOLEAN:
            boolA = a.Boolean();
            break;
        case Type::T_NOT_EXISTS:
            boolA = false;
            break;
        case Type::T_NUMBER:
            boolA = a.Number() != 0.0f;
            break;
        case Type::T_OBJECT:
            boolA = a.Object() != nullptr;
            break;
        case Type::T_SMALL_INT:
            boolA = a.SmallInt() != 0;
            break;
        case Type::T_STRING:
            boolA = a.String() != nullptr && a.String()->length() != 0;
            break;
        case Type::T_SYMBOL:
            boolA = true;
            break;
        case Type::T_UNDEFINED:
            boolA = false;
            break;
        default:
            hydra_trap("Unknown type");
    }

    return boolA;
}

JSObject *GetGlobalObject()
{
    return Global;
}

JSObject *MakeGetterSetterPair(gc::ThreadAllocator &allocator, JSValue getter, JSValue setter)
{
    JSObject *ret = NewEmptyObjectSafe(allocator);
    ret->Set(allocator, strs::GET, getter);
    ret->Set(allocator, strs::SET, setter);

    return ret;
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

static bool lib_Array(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *argument, JSValue &retVal, JSValue &error)
{
    // TODO
    return NewArray(allocator, retVal, error);
}

static bool lib_Array_IsArray(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    JSValue arg;

    if (!ObjectGetSafeArray(allocator, arguments, 0, arg, error))
    {
        return false;
    }

    js_return(JSValue::FromBoolean(lib_Array_IsArraySafe(arg)));
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

static bool lib_Array_prototype_length_get(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    if (!lib_Array_IsArraySafe(thisArg))
    {
        js_throw_error(TypeError, "this is not an array");
    }

    js_return(JSValue::FromSmallInt(dynamic_cast<JSArray*>(thisArg.Object())->GetLength()));
}

static void InitializeArray(gc::ThreadAllocator &allocator)
{
    auto getter = JSValue::FromObject(NewNativeFunc(allocator, lib_Array_prototype_length_get));
    auto setter = JSValue();
    auto pair = JSValue::FromObject(MakeGetterSetterPair(allocator, getter, setter));

    Array_prototype->Set(allocator, strs::LENGTH, pair,
        JSObjectPropertyAttribute::HAS_VALUE | JSObjectPropertyAttribute::IS_ENUMERABLE_MASK);
}

} // namespace semantic
} // namespace runtime
} // namespace hydra