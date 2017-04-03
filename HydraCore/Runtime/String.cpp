#include "String.h"

namespace hydra
{
namespace runtime
{

String *String::Empty(gc::ThreadAllocator &allocator)
{
    return allocator.AllocateAuto<EmptyString>();
}

String *String::Concat(
    gc::ThreadAllocator &allocator,
    String *left,
    String *right)
{
    return allocator.AllocateAuto<ConcatedString>(left, right);
}

String *String::Slice(
    gc::ThreadAllocator &allocator,
    String *sliced,
    size_t start,
    size_t length)
{
    return allocator.AllocateAuto<SlicedString>(sliced, start, length);
}

String *String::Flatten(gc::ThreadAllocator &allocator)
{
    if (Flattenned)
    {
        return Flattenned;
    }

    size_t allocateSize = length() + sizeof(ManagedString);
    auto flattenned = allocator.AllocateWithSizeAuto<ManagedString>(allocateSize, length());

    flatten(0, length(), flattenned->begin());
    Flattenned = flattenned;

    gc::Heap::GetInstance()->WriteBarrier(this, Flattenned);
    return Flattenned;
}

} // namespace runtime
} // namespace hydra