#ifndef _HEAP_OBJECT_H_
#define _HEAP_OBJECT_H_

#include "Common/HydraCore.h"

#include "GCDefs.h"

#include <functional>

namespace hydra
{

namespace gc
{

class Cell
{
public:
    Cell(const Cell &) = delete;
    Cell(Cell &&) = delete;

    Cell & operator = (const Cell &) = delete;
    Cell & operator = (Cell &&) = delete;

    virtual ~Cell() = default;

    inline bool IsInUse() const
    {
        return (Property.load() & IS_IN_USE) != 0;
    }

    inline bool IsLarge() const
    {
        return (Property.load() & IS_LARGE) != 0;
    }

    inline u8 GetGCState() const
    {
        return Property.load() & GC_STATE_MASK;
    }

    inline bool TrySetGCState(u8 &expected, u8 desired)
    {
        hydra_assert((expected & ~GC_STATE_MASK) == 0, "'expected' should be masked by GC_STATE_MASK");
        hydra_assert((desired & ~GC_STATE_MASK) == 0, "'desired' should be masked by GC_STATE_MASK");

        u8 currentProperty = Property.load();

        if ((currentProperty & GC_STATE_MASK) != expected)
        {
            expected = currentProperty & GC_STATE_MASK;
            return false;
        }

        u8 expectedProperty = (currentProperty & ~GC_STATE_MASK) | expected;
        u8 desiredProperty = (currentProperty & ~GC_STATE_MASK) | desired;

        while (!Property.compare_exchange_weak(expectedProperty, desiredProperty))
        {
            if ((expectedProperty & GC_STATE_MASK) != expected)
            {
                expected = expectedProperty & GC_STATE_MASK;
                return false;
            }
            else
            {
                desiredProperty = (expectedProperty & ~GC_STATE_MASK) | desired;
            }
        }

        return true;
    }

    inline u8 SetGCState(u8 desired)
    {
        u8 expected = GetGCState();
        while (!TrySetGCState(expected, desired))
        { }
        return expected;
    }

    inline u8 GetProperty()
    {
        return Property.load(std::memory_order_acquire);
    }

    inline static bool CellIsInUse(u8 property)
    {
        return (property & IS_IN_USE) != 0;
    }

    inline static bool CellIsLarge(u8 property)
    {
        return (property & IS_LARGE) != 0;
    }

    inline static u8 CellGetGCState(u8 property)
    {
        return (property & GC_STATE_MASK);
    }

    inline static void SetNotInUse(Cell *cell)
    {
        u8 currentProperty = cell->Property.load();
        u8 property = currentProperty & ~IS_IN_USE;

        while (!cell->Property.compare_exchange_weak(currentProperty, property))
        {
            if ((currentProperty & IS_IN_USE) == 0)
            {
                break;
            }
            property = currentProperty & ~IS_IN_USE;
        }
    }

    static constexpr u8 IS_IN_USE = 1u << 7;
    static constexpr u8 IS_LARGE = 1u << 6;
    static constexpr u8 GC_STATE_MASK = (1u << 2) - 1;

protected:
    Cell(u8 property)
        : Property(property)
    { }

    std::atomic<u8> Property;
};

class EmptyCell : public Cell
{
public:
    EmptyCell(EmptyCell *next)
        : Cell(0), Next(next)
    { }

    std::atomic<EmptyCell *> Next;
};

static_assert(sizeof(EmptyCell) <= MINIMAL_ALLOCATE_SIZE,
    "EmptyCell should be smaller than MINIMAL_ALLOCATE_SIZE");

class HeapObject : public Cell
{
public:
    HeapObject(u8 property)
        : Cell(property | IS_IN_USE)
    { }

    virtual ~HeapObject()
    {
        SetNotInUse(this);
    }

    virtual void Scan(std::function<void(HeapObject *)>) = 0;
};

} // namespace internal

} // namespace hydra

#endif // _HEAP_OBJECT_H_