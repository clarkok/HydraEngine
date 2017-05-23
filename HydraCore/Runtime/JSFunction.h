#ifndef _JSFUNCTION_H_
#define _JSFUNCTION_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/GC.h"

#include "JSObject.h"
#include "Klass.h"

#include "JSArray.h"
#include "ManagedArray.h"

#include <functional>

namespace hydra
{

namespace vm
{
class Scope;
struct IRFunc;
}

namespace runtime
{

#define js_return(__value)  do { retVal = __value; return true; } while (false)
#define js_throw(__error)   do { error = __error; return false; } while (false)
#define js_call(__retVal, __func, __this, __arguments)                                      \
    do {                                                                                    \
        JSValue __error;                                                                    \
        auto __result = __func->Call(allocator, __this, __arguments, __retVal, __error);    \
        if (!__result) js_throw(__error);                                                   \
    } while (false)

#define js_call_catched(__retVal, __func, __this, __arguments, __catched_block, __error)    \
    do {                                                                                    \
        auto __result = __func->Call(allocator, __this, __arguments, __retVal, __error);    \
        if (!__result) { __catched_block; }                                                 \
    } while (false)

#define return_js_call(__func, __this, __arguments)                                     \
    return __func->Call(allocator, __this, __arguments, retVal, error)

#define lib_call(__retVal, __func, __this, __arguments)                                 \
    do {                                                                                \
        JSValue __error;                                                                \
        auto __result = __func(allocator, __this, __arguments, __retVal, __error);      \
        if (!__result) js_throw(__error);                                               \
    } while (false)

#define lib_call_catched(__retVal, __func, __this, __arguments, __catched_block, __error)   \
    do {                                                                                    \
        auto __result = __func(allocator, __this, __arguments, __retVal, __error);          \
        if (!__result) { __catched_block; }                                                 \
    } while (false)

#define return_lib_call(__func, __this, __arguments)                                     \
    return __func(allocator, __this, __arguments, retVal, error)

class JSFunction : public JSObject
{
public:
    JSFunction(u8 property, runtime::Klass *klass, Array *table)
        : JSObject(property, klass, table)
    { }

    virtual bool Call(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error) = 0;
};

class JSNativeFunction : public JSFunction
{
public:
    using Functor = std::function<bool(gc::ThreadAllocator&, JSValue, JSArray*, JSValue&, JSValue&)>;

    JSNativeFunction(u8 property, runtime::Klass *klass, Array *table, Functor func)
        : JSFunction(property, klass, table),
        Func(func)
    { }

    virtual bool Call(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error) override final;

private:
    Functor Func;
};

class JSCompiledFunction : public JSFunction
{
public:
    JSCompiledFunction(u8 property, runtime::Klass *klass, Array *table, vm::Scope *scope, RangeArray *captured, vm::IRFunc *func)
        : JSFunction(property, klass, table), Scope(scope), Captured(captured), Func(func)
    { }

    virtual void Scan(std::function<void(gc::HeapObject*)> scan) override final;

    virtual bool Call(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error) override final;

private:
    vm::Scope *Scope;
    RangeArray *Captured;
    vm::IRFunc *Func;
};

class JSCompiledArrowFunction : public JSFunction
{
public:
    JSCompiledArrowFunction(u8 property, runtime::Klass *klass, Array *table, vm::Scope *scope, RangeArray *captured, vm::IRFunc *func)
        : JSFunction(property, klass, table), Scope(scope), Captured(captured), Func(func)
    { }

    virtual void Scan(std::function<void(gc::HeapObject*)> scan) override final;

    virtual bool Call(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error) override final;

private:
    vm::Scope *Scope;
    RangeArray *Captured;
    vm::IRFunc *Func;
};

} // namespace runtime
} // namespace hydra

#endif // _JSFUNCTION_H_