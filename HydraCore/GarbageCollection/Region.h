#ifndef _REGION_H_
#define _REGION_H_

#include "Common/HydraCore.h"
#include "Common/ConcurrentLinkedList.h"
#include "Common/ConcurrentLevelHashSet.h"
#include "Common/Constexpr.h"

#include "GCDefs.h"
#include "HeapObject.h"

#include <iterator>

namespace hydra
{

namespace gc
{

class Region : public concurrent::ForwardLinkedListNode<class Region>
{
public:
    struct iterator : std::iterator<std::bidirectional_iterator_tag, Cell *>
    {
    public:
        iterator()
            : Owner(nullptr), Level(0), CellSize(0), Offset(0)
        { }

        iterator(Region *owner)
            : Owner(owner),
            Level(owner->Level),
            CellSize(CellSizeFromLevel(Level)),
            Offset(AllocateBegin(Level))
        { }

        iterator(Region *owner, size_t offset)
            : Owner(owner),
            Level(owner->Level),
            CellSize(CellSizeFromLevel(Level)),
            Offset(offset)
        {
            hydra_assert((offset % CellSize) == 0, "'offset' should align to CellSize");
        }

        iterator(const iterator &) = default;
        iterator(iterator &&) = default;
        ~iterator() = default;

        iterator & operator = (const iterator &) = default;
        iterator & operator = (iterator &&) = default;

        inline bool operator == (iterator other) const
        {
            return Owner == other.Owner &&
                Offset == other.Offset;
        }

        inline bool operator != (iterator other) const
        {
            return !operator == (other);
        }

        Cell * operator *() const
        {
            return reinterpret_cast<Cell *>(
                reinterpret_cast<uintptr_t>(Owner) + Offset);
        }

        iterator & operator ++()
        {
            if (Owner && CellSize && (Offset < AllocateEnd(Level)))
            {
                Offset += CellSize;
            }

            return *this;
        }

        iterator operator ++(int)
        {
            auto ret = *this;
            operator++();
            return ret;
        }

        iterator & operator --()
        {
            if (Owner && CellSize && (Offset > AllocateBegin(Level)))
            {
                Offset -= CellSize;
            }

            return *this;
        }

        iterator operator --(int)
        {
            auto ret = *this;
            operator--();
            return ret;
        }

    private:
        Region *Owner;
        size_t Level;
        size_t CellSize;
        size_t Offset;
    };

    virtual ~Region();

    template <typename T, typename ...T_Args>
    T *Allocate(T_Args ...args)
    {
        static_assert(std::is_base_of<Cell, T>::value,
            "T must inhert from Cell");
        hydra_assert(sizeof(T) <= CellSizeFromLevel(Level),
            "sizeof(T) must be smaller than CellSize");

        if (Allocated <= AllocateEnd(Level))
        {
            if (Allocated == AllocateEnd(Level))
            {
                return nullptr;
            }

            Cell *cell = reinterpret_cast<Cell *>(
                reinterpret_cast<uintptr_t>(this) + Allocated);

            Allocated += CellSizeFromLevel(Level);

            hydra_assert(!cell->IsInUse(),
                "Cell must not be in use");

            return new (cell) T(Cell::IS_IN_USE, args...);
        }
        else
        {
            Cell *cell = FreeHead;
            FreeHead = FreeHead->Next;

            hydra_assert(!cell->IsInUse(),
                "Cell must not be in use");

            return new (cell) T(Cell::IS_IN_USE, args...);
        }
    }

    inline size_t IncreaseOldObjectCount()
    {
        return OldObjectCount.fetch_add(1);
    }

    static inline Region *GetRegionOfObject(HeapObject *obj)
    {
        return reinterpret_cast<Region *>(reinterpret_cast<uintptr_t>(obj) & ~(REGION_SIZE - 1));
    }

    size_t YoungSweep();
    size_t FullSweep();

    inline iterator begin()
    {
        return iterator(this);
    }

    inline iterator end()
    {
        return iterator(this, AllocateEnd(Level));
    }

    void FreeAll();

    inline static Region *New(size_t level)
    {
        Region *region = NewInternal(level);
        RegionSet.Add(region);
        return region;
    }

    inline static void Delete(Region *region)
    {
        RegionSet.Remove(region);
        DeleteInternal(region);
    }

    static constexpr size_t CellSizeFromLevel(size_t level)
    {
        return 1ull << (level + MINIMAL_ALLOCATE_SIZE_LEVEL);
    }

    static constexpr size_t CellCountFromLevel(size_t level)
    {
        return (AllocateEnd(level) - AllocateBegin(level)) / CellSizeFromLevel(level);
    }

    static constexpr size_t AllocateBegin(size_t level)
    {
        return (sizeof(Region) + CellSizeFromLevel(level) - 1) & ~(CellSizeFromLevel(level) - 1);
    }

    static constexpr size_t AllocateEnd(size_t level)
    {
        return REGION_SIZE;
    }

    inline static size_t GetTotalRegionCount()
    {
        return TotalRegionCount.load(std::memory_order_relaxed);
    }

    inline u64 Hash() const
    {
        u64 hash = reinterpret_cast<u64>(this);
        return hash | cexpr::SubBits(
            hash,
            cexpr::TypeBitCount<u64>::value - REGION_SIZE_LEVEL,
            REGION_SIZE_LEVEL);
    }

    static bool IsInRegion(void *ptr, Cell *&cell);

private:
    Region(size_t level);

    size_t Level;

    union
    {
        size_t Allocated;
        EmptyCell *FreeHead;
    };

    std::atomic<size_t> OldObjectCount;

    static Region *NewInternal(size_t level);
    static void DeleteInternal(Region *);

    static std::atomic<size_t> TotalRegionCount;
    static concurrent::ForwardLinkedList<Region> FreeRegions;
    static concurrent::LevelHashSet<Region> RegionSet;

    friend class Heap;
};

}

}

#endif // _REGION_H_