#include "Region.h"
#include "Heap.h"

#include "Common/Platform.h"

namespace hydra
{
namespace gc
{

std::atomic<size_t> Region::TotalRegionCount { 0 };
concurrent::ForwardLinkedList<Region> Region::FreeRegions;
concurrent::LevelHashSet<Region> Region::RegionSet;

size_t Region::YoungSweep()
{
    Logger::GetInstance()->Log() << "Region " << this <<
        " before Young Sweep: " << OldObjectCount.load();

    if (OldObjectCount.load() == 0)
    {
        for (auto cell : *this)
        {
            hydra_assert(cell->GetGCState() == GCState::GC_WHITE,
                "All objects in Region should be WHITE");
            cell->~Cell();
        }

        Allocated = AllocateBegin(Level);

        Logger::GetInstance()->Log() << "Region " << this <<
            " after Young Sweep: 0";
        return 0;
    }

    size_t oldObjectCount = 0;
    Allocated = AllocateEnd(Level);

    for (auto cell : *this)
    {
        if (cell->IsInUse() && cell->GetGCState() == GCState::GC_WHITE)
        {
            cell->~Cell();
            FreeHead = new (cell)EmptyCell(FreeHead);
        }
        else if (!cell->IsInUse())
        {
            FreeHead = new (cell)EmptyCell(FreeHead);
        }
        else
        {
            hydra_assert(cell->GetGCState() != GCState::GC_GREY,
                "All live objects should be marked");
            oldObjectCount++;
        }
    }

    hydra_assert(oldObjectCount == OldObjectCount.load(),
        "OldObjectCount should match");

    Logger::GetInstance()->Log() << "Region " << this <<
        " after Young Sweep: " << oldObjectCount;
    return oldObjectCount;
}

size_t Region::FullSweep()
{
    size_t oldObjectCount = 0;
    Allocated = AllocateEnd(Level);

    for (auto cell : *this)
    {
        if (cell->IsInUse() && cell->GetGCState() != GCState::GC_BLACK)
        {
            hydra_assert(cell->GetGCState() != GCState::GC_GREY,
                "GC state cannot be GREY at this point");

            cell->~Cell();
            FreeHead = new (cell)EmptyCell(FreeHead);
        }
        else if (cell->IsInUse())
        {
            hydra_assert(cell->GetGCState() != GCState::GC_GREY,
                "GC state cannot be GREY at this point");

            cell->SetGCState(GCState::GC_DARK);
            oldObjectCount++;
        }
    }

    OldObjectCount.store(oldObjectCount, std::memory_order_relaxed);
    return oldObjectCount;
}

Region::Region(size_t level)
    : ForwardLinkedListNode(), Level(level), Allocated(AllocateBegin(level)), OldObjectCount(0)
{
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

    if (!RegionSet.Has(region))
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