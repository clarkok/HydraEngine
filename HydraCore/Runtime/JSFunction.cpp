#include "JSFunction.h"

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

void JSCompiledFunction::Scan(std::function<void(gc::HeapObject*)> scan)
{
    if (Scope) scan(Scope);
    if (Captured) scan(Captured);
}

bool JSCompiledFunction::Call(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    Array *regs = Array::New(allocator, Func->GetRegisterCount());
    Array *table = Array::New(allocator, Func->GetVarCount());

    std::vector<JSValue *> captured;
    for (size_t i = 0; i < Captured->Capacity(); ++i)
    {
        hydra_assert(JSValue::GetType(Captured->at(i)) == Type::T_SMALL_INT,
            "Captured must be T_SMALL_INT");
        captured.push_back(&Scope->GetRegs()->at(Captured->at(i).SmallInt()));
    }

    Array *arrayArgs = Array::New(allocator, arguments->GetLength());
    for (size_t i = 0; i < arguments->GetLength(); ++i)
    {
        JSValue value;
        JSObjectPropertyAttribute attribute;

        if (arguments->Get(i, value, attribute))
        {
            arrayArgs->at(i) = value;
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
        captured,
        thisArg,
        arrayArgs);

    return Func->Call(allocator, newScope, retVal, error);
}

void JSCompiledArrowFunction::Scan(std::function<void(gc::HeapObject*)> scan)
{
    if (Scope) scan(Scope);
    if (Captured) scan(Captured);
}

bool JSCompiledArrowFunction::Call(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    Array *regs = Array::New(allocator, Func->GetRegisterCount());
    Array *table = Array::New(allocator, Func->GetVarCount());

    std::vector<JSValue *> captured;
    for (size_t i = 0; i < Captured->Capacity(); ++i)
    {
        hydra_assert(JSValue::GetType(Captured->at(i)) == Type::T_SMALL_INT,
            "Captured must be T_SMALL_INT");
        captured.push_back(&Scope->GetRegs()->at(Captured->at(i).SmallInt()));
    }

    Array *arrayArgs = Array::New(allocator, arguments->GetLength());
    for (size_t i = 0; i < arguments->GetLength(); ++i)
    {
        JSValue value;
        JSObjectPropertyAttribute attribute;

        if (arguments->Get(i, value, attribute))
        {
            arrayArgs->at(i) = value;
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
        captured,
        Scope->GetThisArg(),
        arrayArgs);

    return Func->Call(allocator, newScope, retVal, error);
}

} // namespace runtime
} // namespace hydra