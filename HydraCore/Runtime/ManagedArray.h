#ifndef _MANAGED_ARRAY_H_
#define _MANAGED_ARRAY_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/GC.h"
#include "Type.h"

namespace hydra
{
namespace runtime
{

class Array : public gc::HeapObject
{
public:
    inline static size_t CapacityFromLevel(size_t level)
    {
        return (gc::Region::CellSizeFromLevel(level) - sizeof(Array)) / sizeof(JSValue);
    }

    inline static size_t LevelFromCapacity(size_t capacity)
    {
        return gc::Region::GetLevelFromSize(sizeof(Array) + sizeof(JSValue) * capacity);
    }

    inline size_t Capacity() const
    {
        return CapacityFromLevel(Level);
    }

    inline size_t GetLevel() const
    {
        return Level;
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

    static Array *New(gc::ThreadAllocator &allocator, size_t capacity)
    {
        return NewOfLevel(allocator, LevelFromCapacity(capacity));
    }

    static Array *NewOfLevel(gc::ThreadAllocator &allocator, size_t level)
    {
        return allocator.AllocateWithSizeAuto<Array>(
            gc::Region::CellSizeFromLevel(level), level);
    }

    inline static size_t OffsetTable()
    {
        return sizeof(Array);
    }

protected:
    Array(u8 property, size_t level)
        : HeapObject(property), Level(level)
    {
        std::fill(begin(), end(), JSValue());
    }

    JSValue *Table() const
    {
        return reinterpret_cast<JSValue *>(
            reinterpret_cast<uintptr_t>(this) + sizeof(Array));
    }

    JSValue *TableLimit() const
    {
        return Table() + Capacity();
    }

    size_t Level;

    friend class gc::Region;
    friend class gc::ThreadAllocator;
};

class RangeArray : public gc::HeapObject
{
public:
    inline static size_t CapacityFromLevel(size_t level)
    {
        return (gc::Region::CellSizeFromLevel(level) - sizeof(RangeArray)) / sizeof(JSValue);
    }

    inline static size_t LevelFromCapacity(size_t capacity)
    {
        return gc::Region::GetLevelFromSize(sizeof(RangeArray) + sizeof(JSValue) * capacity);
    }

    inline size_t GetLevel() const
    {
        return Level;
    }

    inline size_t GetLength() const
    {
        return Length;
    }

    inline JSValue &at(size_t index)
    {
        hydra_assert(index < Length, "Out of range");
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

    static RangeArray *New(gc::ThreadAllocator &allocator, size_t length)
    {
        return NewOfLevel(allocator, LevelFromCapacity(length), length);
    }

    static RangeArray *NewOfLevel(gc::ThreadAllocator &allocator, size_t level, size_t length)
    {
        return allocator.AllocateWithSizeAuto<RangeArray>(
            gc::Region::CellSizeFromLevel(level), level, length);
    }

    inline static size_t OffsetTable()
    {
        return sizeof(RangeArray);
    }

private:
    RangeArray(u8 property, size_t level, size_t length)
        : HeapObject(property), Level(level), Length(length)
    {
        std::fill(begin(), end(), JSValue());
    }

    JSValue *Table() const
    {
        return reinterpret_cast<JSValue *>(
            reinterpret_cast<uintptr_t>(this) + sizeof(RangeArray));
    }

    JSValue *TableLimit() const
    {
        return Table() + Length;
    }

    inline size_t Capacity() const
    {
        return CapacityFromLevel(Level);
    }

    size_t Level;
    size_t Length;

    friend class gc::Region;
    friend class gc::ThreadAllocator;
};

} // namespace runtime
} // namespace hydra

#endif // _MANAGED_ARRAY_H_