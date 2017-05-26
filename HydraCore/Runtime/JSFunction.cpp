#include "JSFunction.h"

#include "VirtualMachine/IR.h"
#include "VirtualMachine/Scope.h"
#include "VirtualMachine/CompiledFunction.h"

#include <vector>

namespace hydra
{
namespace runtime
{

bool JSNativeFunction::Call(gc::ThreadAllocator &allocator,
    JSValue thisArg,
    JSArray *arguments,
    JSValue &retVal,
    JSValue &error)
{
    return Func(allocator, thisArg, arguments, retVal, error);
}

JSCompiledFunction::JSCompiledFunction(u8 property, runtime::Klass *klass, Array *table, vm::Scope *scope, RangeArray *captured, vm::IRFunc *func)
    : JSFunction(property, klass, table), Scope(scope), Captured(captured), Func(func)
{
    gc::Heap::GetInstance()->WriteBarrier(this, Scope);
    gc::Heap::GetInstance()->WriteBarrier(this, Captured);
}

void JSCompiledFunction::Scan(std::function<void(gc::HeapObject*)> scan)
{
    JSObject::Scan(scan);
    if (Scope) scan(Scope);
    if (Captured) scan(Captured);
}

bool JSCompiledFunction::Call(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    auto compiled = Func->Compiled.load();
    if (!compiled)
    {
        compiled = Func->BaselineFuture.get();
    }

    Array *regs = Array::New(allocator, compiled->GetRegisterCount());
    Array *table = Array::New(allocator, compiled->GetVarCount());

    RangeArray *arrayArgs = RangeArray::New(allocator, arguments->GetLength());
    for (size_t i = 0; i < arguments->GetLength(); ++i)
    {
        JSValue value;
        JSObjectPropertyAttribute attribute;

        if (arguments->Get(i, value, attribute))
        {
            arrayArgs->at(i) = value;
            if (value.IsReference())
            {
                gc::Heap::GetInstance()->WriteBarrier(arrayArgs, value.ToReference());
            }
        }
        else
        {
            arrayArgs->at(i) = JSValue();
        }
    }

    vm::Scope *newScope = allocator.AllocateAuto<vm::Scope>(
        Scope,
        regs,
        table,
        Captured,
        thisArg,
        arrayArgs);

    vm::AutoThreadTop autoThreadTop(newScope);
    return compiled->Call(allocator, newScope, retVal, error);
}

JSCompiledArrowFunction::JSCompiledArrowFunction(u8 property, runtime::Klass *klass, Array *table, vm::Scope *scope, RangeArray *captured, vm::IRFunc *func)
    : JSFunction(property, klass, table), Scope(scope), Captured(captured), Func(func)
{
    gc::Heap::GetInstance()->WriteBarrier(this, Scope);
    gc::Heap::GetInstance()->WriteBarrier(this, Captured);
}

void JSCompiledArrowFunction::Scan(std::function<void(gc::HeapObject*)> scan)
{
    JSObject::Scan(scan);
    if (Scope) scan(Scope);
    if (Captured) scan(Captured);
}

bool JSCompiledArrowFunction::Call(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    auto compiled = Func->Compiled.load();
    if (!compiled)
    {
        compiled = Func->BaselineFuture.get();
    }

    Array *regs = Array::New(allocator, compiled->GetRegisterCount());
    Array *table = Array::New(allocator, compiled->GetVarCount());

    RangeArray *arrayArgs = RangeArray::New(allocator, arguments->GetLength());
    for (size_t i = 0; i < arguments->GetLength(); ++i)
    {
        JSValue value;
        JSObjectPropertyAttribute attribute;

        if (arguments->Get(i, value, attribute))
        {
            arrayArgs->at(i) = value;
            if (value.IsReference())
            {
                gc::Heap::GetInstance()->WriteBarrier(arrayArgs, value.ToReference());
            }
        }
        else
        {
            arrayArgs->at(i) = JSValue();
        }
    }

    vm::Scope *newScope = allocator.AllocateAuto<vm::Scope>(
        Scope,
        regs,
        table,
        Captured,
        Scope->GetThisArg(),
        arrayArgs);

    vm::AutoThreadTop autoThreadTop(newScope);
    return compiled->Call(allocator, newScope, retVal, error);
}

} // namespace runtime
} // namespace hydra