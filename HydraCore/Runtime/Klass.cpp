#include "Klass.h"

namespace hydra
{
namespace runtime
{

Klass *Klass::EmptyKlass(gc::ThreadAllocator &allocator)
{
    return allocator.AllocateWithSizeAuto<Klass>(KlassObjectSizeOfLevel(0), 0);
}

} // runtime
} // hydra