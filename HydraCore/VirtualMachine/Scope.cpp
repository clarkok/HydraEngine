#include "Scope.h"

namespace hydra
{
namespace vm
{

Scope::JSValue *Scope::AllocateStatic(Scope *scope)
{
    return scope->Allocate();
}

void Scope::Scan(std::function<void(gc::HeapObject *)> scan)
{
    if (Regs) scan(Regs);
    if (Upper) scan(Upper);
    if (Captured) scan(Captured);
    if (ThisArg.IsReference() && ThisArg.ToReference())
    {
        scan(ThisArg.ToReference());
    }
    if (Arguments) scan(Arguments);
}

} // namespace vm
} // namespace hydra