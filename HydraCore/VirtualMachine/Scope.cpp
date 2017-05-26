#include "Scope.h"

namespace hydra
{
namespace vm
{

thread_local Scope *Scope::ThreadTop;

Scope::JSValue *Scope::AllocateStatic(Scope *scope)
{
    return scope->Allocate();
}

void Scope::Scan(std::function<void(gc::HeapObject *)> scan)
{
    if (Upper) scan(Upper);
    if (Regs) scan(Regs);
    if (Table) scan(Table);
    if (Captured) scan(Captured);
    if (ThisArg.IsReference() && ThisArg.ToReference())
    {
        scan(ThisArg.ToReference());
    }
    if (Arguments) scan(Arguments);
}

void Scope::Safepoint(Scope *scope)
{
    auto heap = gc::Heap::GetInstance();

    if (scope->Regs)
    {
        for (auto &value : *(scope->Regs))
        {
            if (value.IsReference() && value.ToReference())
            {
                heap->Remember(value.ToReference());
            }
        }
    }

    if (scope->Table)
    {
        for (auto &value : *(scope->Regs))
        {
            if (value.IsReference() && value.ToReference())
            {
                heap->Remember(value.ToReference());
            }
        }
    }

    if (scope->Captured)
    {
        for (auto &value : *(scope->Captured))
        {
            if (value.IsReference() && value.ToReference())
            {
                heap->Remember(value.ToReference());
            }
        }
    }

    if (scope->Arguments)
    {
        for (auto &value : *(scope->Arguments))
        {
            if (value.IsReference() && value.ToReference())
            {
                heap->Remember(value.ToReference());
            }
        }
    }

    if (scope->ThisArg.IsReference() && scope->ThisArg.ToReference())
    {
        heap->Remember(scope->ThisArg.ToReference());
    }
}

} // namespace vm
} // namespace hydra