#include "Klass.h"

namespace hydra
{
namespace runtime
{

Klass *Klass::EmptyKlass(gc::ThreadAllocator &allocator)
{
    return allocator.AllocateWithSizeAuto<Klass>(KlassObjectSizeOfLevel(0), 0);
}

JSObject *Klass::NewObject(gc::ThreadAllocator &allocator)
{
    return allocator.AllocateWithSizeAuto<JSObject>(
        gc::Region::CellSizeFromLevel(Level), this);
}

} // runtime
} // hydra