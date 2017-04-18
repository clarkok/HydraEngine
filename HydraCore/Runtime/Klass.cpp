#include "Klass.h"

namespace hydra
{
namespace runtime
{

Klass *Klass::EmptyKlass(gc::ThreadAllocator &allocator)
{
    return allocator.AllocateAuto<Klass>(gc::Region::GetLevelFromSize(sizeof(Klass)));
}

} // runtime
} // hydra