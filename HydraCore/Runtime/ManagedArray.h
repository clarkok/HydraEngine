#ifndef _MANAGED_ARRAY_H_
#define _MANAGED_ARRAY_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/GC.h"
#include "Type.h"

namespace hydra
{
namespace runtime
{

class ManagedArray : public gc::HeapObject
{
public:
    inline static size_t CapacityFromLevel(size_t level)
    {
        return (gc::Region::CellSizeFromLevel(level) - sizeof(ManagedArray)) / sizeof(JSValue);
    }

    inline static size_t LevelFromCapacity(size_t capacity)
    {
        return gc::Region::GetLevelFromSize(sizeof(ManagedArray) + sizeof(JSValue) * capacity);
    }

    inline size_t Capacity() const
    {
        return CapacityFromLevel(Level);
    }

    inline JSValue &at(size_t index)
    {
        hydra_assert(index < Capacity(), "Out of range");
        return Table()[index];
    }

    inline JSValue *begin()
    {
        return Table();
    }

    inline JSValue *end()
    {
        return TableLimit();
    }

    virtual void Scan(std::function<void(gc::HeapObject*)> scan) override final
    {
        for (auto value : *this)
        {
            if (value.IsReference())
            {
                scan(value.ToReference());
            }
        }
    }

    static ManagedArray *New(gc::ThreadAllocator &allocator, size_t capacity)
    {
        return NewOfLevel(allocator, LevelFromCapacity(capacity));
    }

    static ManagedArray *NewOfLevel(gc::ThreadAllocator &allocator, size_t level)
    {
        return allocator.AllocateWithSizeAuto<ManagedArray>(
            gc::Region::CellSizeFromLevel(level), level);
    }

private:
    ManagedArray(u8 property, size_t level)
        : HeapObject(property), Level(level)
    { }

    JSValue *Table() const
    {
        return reinterpret_cast<JSValue *>(
            reinterpret_cast<uintptr_t>(this) + sizeof(ManagedArray));
    }

    JSValue *TableLimit() const
    {
        return Table() + Capacity();
    }

    size_t Level;

    friend class gc::Region;
};

} // namespace runtime
} // namespace hydra

#endif // _MANAGED_ARRAY_H_