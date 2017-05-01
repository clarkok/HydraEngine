#ifndef _JSFUNCTION_H_
#define _JSFUNCTION_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/GC.h"

#include "JSObject.h"
#include "Klass.h"

#include "JSArray.h"

#include <functional>

namespace hydra
{
namespace runtime
{

class JSFunction : public JSObject
{
public:
    JSFunction(u8 property, runtime::Klass *klass, Array *table)
        : JSObject(property, klass, table)
    { }

    virtual bool Call(JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error) = 0;
};

class JSNativeFunction : public JSFunction
{
public:
    using Functor = std::function<JSValue(JSValue, JSArray *)>;

    JSNativeFunction(u8 property, runtime::Klass *klass, Array *table, Functor func)
        : JSFunction(property, klass, table),
        Func(func)
    { }

    virtual bool Call(JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error) override final;

private:
    Functor Func;
};

} // namespace runtime
} // namespace hydra

#endif // _JSFUNCTION_H_