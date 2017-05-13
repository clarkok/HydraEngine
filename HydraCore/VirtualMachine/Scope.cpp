#include "Scope.h"

namespace hydra
{
namespace vm
{

void Scope::Scan(std::function<void(gc::HeapObject *)> scan)
{
    if (Regs) scan(Regs);
    if (Upper) scan(Upper);
    if (ThisArg.IsReference() && ThisArg.ToReference())
    {
        scan(ThisArg.ToReference());
    }
    if (Arguments) scan(Arguments);
}

} // namespace vm
} // namespace hydra