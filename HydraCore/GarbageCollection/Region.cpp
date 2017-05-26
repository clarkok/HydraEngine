#include "Region.h"
#include "Heap.h"

#include "Common/Platform.h"

#include <cstdlib>

namespace hydra
{
namespace gc
{

std::atomic<size_t> Region::TotalRegionCount { 0 };
concurrent::ForwardLinkedList<Region> Region::FreeRegions;
concurrent::LevelHashSet<Region> Region::RegionSet;

size_t Region::YoungSweep()
{
    Logger::GetInstance()->Log() << "YoungSweep: " << this;
    auto perfSessoin = Logger::GetInstance()->Perf("YoungSweep");

    if (OldObjectCount.load() == 0)
    {
        for (auto cell : *this)
        {
            hydra_assert(cell->IsInUse(),
                "All objects must be in use");
            hydra_assert(cell->GetGCState() == GCState::GC_WHITE,
                "All objects in Region should be WHITE");
            cell->~Cell();
        }

        Allocated = AllocateBegin(Level);
        Logger::GetInstance()->Log() << "Region " << this << " clear";

        return 0;
    }

    size_t oldObjectCount = 0;
    Allocated = AllocateEnd(Level);

    for (auto cell : *this)
    {
        u8 currentProperty = cell->GetProperty();

        if (Cell::CellIsInUse(currentProperty) && Cell::CellGetGCState(currentProperty) == GCState::GC_WHITE)
        {
            cell->~Cell();
            FreeHead = new (cell)EmptyCell(FreeHead);
        }
        else if (!Cell::CellIsInUse(currentProperty))
        {
            // can be grey here
            FreeHead = new (cell)EmptyCell(FreeHead);
        }
        else
        {
            oldObjectCount++;
        }
    }

    hydra_assert(oldObjectCount == OldObjectCount.load(),
        "OldObjectCount should match");
    return oldObjectCount;
}

size_t Region::FullSweep()
{
    Logger::GetInstance()->Log() << "FullSweep: " << this;
    auto perfSessoin = Logger::GetInstance()->Perf("FullSweep");

    size_t oldObjectCount = 0;
    Allocated = AllocateEnd(Level);

    for (auto cell : *this)
    {
        u8 currentProperty = cell->GetProperty();

        if (Cell::CellIsInUse(currentProperty) &&
            (Cell::CellGetGCState(currentProperty) == GCState::GC_WHITE ||
             Cell::CellGetGCState(currentProperty) == GCState::GC_DARK))
        {
            cell->~Cell();
            FreeHead = new (cell)EmptyCell(FreeHead);
        }
        else if (Cell::CellIsInUse(currentProperty))
        {
            // can be grey here
            /*
            */
            oldObjectCount++;
        }
        else
        {
            FreeHead = new (cell)EmptyCell(FreeHead);
        }
    }

    OldObjectCount.store(oldObjectCount, std::memory_order_relaxed);

    if (oldObjectCount == 0)
    {
        Allocated = AllocateBegin(Level);
    }

    return oldObjectCount;
}

void Region::RemarkBlockObject()
{
    Logger::GetInstance()->Log() << "RemarkRegion: " << this;
    auto perfSessoin = Logger::GetInstance()->Perf("Remark");

    auto checker = [](HeapObject *ref)
    {
        hydra_assert(ref, "ref cannot be null");
        hydra_assert(ref->IsInUse(), "ref must be in use");
    };

    for (auto cell : *this)
    {
        auto currentProperty = cell->GetProperty();
        if (Cell::CellIsInUse(currentProperty) && Cell::CellGetGCState(currentProperty) == GCState::GC_BLACK)
        {
            dynamic_cast<HeapObject*>(cell)->Scan(checker);

            u8 gcState = GCState::GC_BLACK;
            while (gcState == GCState::GC_BLACK)
            {
                if (cell->TrySetGCState(gcState, GCState::GC_DARK))
                {
                    break;
                }
            }
        }
    }
}

Region::Region(size_t level)
    : ForwardLinkedListNode(), Level(level), Allocated(AllocateBegin(level)), OldObjectCount(0)
{
    std::memset(
        reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(this) + AllocateBegin(Level)),
        0,
        REGION_SIZE - AllocateBegin(Level)
    );
}

Region::~Region()
{
    FreeAll();
}

void Region::FreeAll()
{
    for (auto cell : *this)
    {
        if (cell->IsInUse())
        {
            cell->~Cell();
        }
    }
}

bool Region::IsInRegion(void *ptr, Cell *&cell)
{
    Region *region = GetRegionOfObject(reinterpret_cast<HeapObject*>(ptr));

    if (!region || !RegionSet.Has(region))
    {
        return false;
    }

    uintptr_t offset = reinterpret_cast<uintptr_t>(ptr) -
        reinterpret_cast<uintptr_t>(region);

    if (offset < AllocateBegin(region->Level))
    {
        cell = nullptr;
    }
    else
    {
        auto cellSize = CellSizeFromLevel(region->Level);
        auto cellOffset = offset & (~cellSize + 1);
        cell = reinterpret_cast<Cell*>(reinterpret_cast<uintptr_t>(region) + cellOffset);
    }

    return true;
}

Region * Region::NewInternal(size_t level)
{
    TotalRegionCount.fetch_add(1, std::memory_order_relaxed);

    Region *node = nullptr;

    if (FreeRegions.Pop(node))
    {
        return new (node) Region(level);
    }

    void *allocated = platform::AlignedAlloc(REGION_SIZE, REGION_SIZE);
    std::memset(allocated, 0, REGION_SIZE);

    Region *region = new (allocated) Region(level);

    hydra_assert(reinterpret_cast<uintptr_t>(allocated) % REGION_SIZE == 0,
        "'allocated' not aligned");
    hydra_assert(reinterpret_cast<uintptr_t>(region) == reinterpret_cast<uintptr_t>(allocated),
        "'region' should be the same with 'allocated'");

    return region;
}

void Region::DeleteInternal(Region *region)
{
    TotalRegionCount.fetch_add(-1, std::memory_order_relaxed);

    region->~Region();

    if (FreeRegions.GetCount() >= CACHED_FREE_REGION_COUNT)
    {
        platform::AlignedFree(region);
    }
    else
    {
        FreeRegions.Push(region);
    }
}

} // namespace gc
} // namespace hydra