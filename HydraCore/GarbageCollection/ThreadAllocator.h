#ifndef _THREAD_ALLOCATOR_H_
#define _THREAD_ALLOCATOR_H_

#include "Common/HydraCore.h"

#include "Heap.h"
#include "Region.h"
#include "GCDefs.h"

#include "Common/Platform.h"
#include "Heap.h"

#include <algorithm>
#include <array>

namespace hydra
{
namespace gc
{

class ThreadAllocator
{
public:
    explicit ThreadAllocator(Heap *owner)
        : Owner(owner),
        ReportedGCRound(0),
        RunningLock(Owner->RunningMutex),
        Active(true)
    {
        LocalPool.fill(nullptr);

        Owner->TotalThreads.fetch_add(1, std::memory_order_relaxed);
    }

    ~ThreadAllocator()
    {
        Owner->TotalThreads.fetch_add(-1, std::memory_order_relaxed);
    }

    template <typename T, typename T_Report, typename ...T_Args>
    T* Allocate(T_Report reportFunc, T_Args ...args)
    {
        return AllocateWithSize<T>(sizeof(T), reportFunc, args...);
    }

    template <typename T, typename T_Report, typename ...T_Args>
    T* AllocateWithSize(size_t size, T_Report reportFunc, T_Args ...args)
    {
        static_assert(std::is_base_of<HeapObject, T>::value,
            "T should inhert from HeapObject");

        Checkpoint(reportFunc);

        if (size > MAXIMAL_ALLOCATE_SIZE)
        {
            trap("TODO Allocate large object not implemented");
        }

        size_t level = GetLevelFromSize(size);
        if (!LocalPool[level])
        {
            LocalPool[level] = Owner->GetFreeRegion(level);
        }

        T* allocated = LocalPool[level]->Allocate<T>(args...);

        if (!allocated)
        {
            do
            {
                Owner->CommitFullRegion(LocalPool[level]);
                allocated = LocalPool[level]->Allocate<T>(args...);
            } while (!allocated);
        }

        return allocated;
    }

    template <typename T_Report>
    void SetInactive(T_Report reportFunc)
    {
        if (Active.exchange(false))
        {
            reportFunc();
            Owner->TotalThreads.fetch_add(-1, std::memory_order_relaxed);
            RunningLock.unlock();
        }
    }

    inline void SetActive()
    {
        if (!Active.exchange(true))
        {
            Owner->TotalThreads.fetch_add(1, std::memory_order_relaxed);
            RunningLock.lock();
        }
    }

    static inline size_t GetLevelFromSize(size_t size)
    {
        hydra_assert(size <= MAXIMAL_ALLOCATE_SIZE, "'size' is too large");

        if (size < MINIMAL_ALLOCATE_SIZE)
        {
            return 0;
        }

        return (platform::GetMSB(size - 1) + 1) - MINIMAL_ALLOCATE_SIZE_LEVEL;
    }

    template <typename T_Report>
    inline void Checkpoint(T_Report reportFunc)
    {
        size_t currentGCRound = Owner->GCRount.load(std::memory_order_acquire);
        if (ReportedGCRound != currentGCRound)
        {
            ReportedGCRound = currentGCRound;
            reportFunc();
            Owner->ReportedThreads.fetch_add(1);
        }
        else if (Owner->PauseRequested.load(std::memory_order_acquire))
        {
            reportFunc();
            Owner->WakeupCV.wait(RunningLock,
                [this]() { return !Owner->PauseRequested.load(std::memory_order_acquire); });
        }
    }

private:
    Heap *Owner;
    std::array<Region *, LEVEL_NR> LocalPool;
    size_t ReportedGCRound;
    std::shared_lock<std::shared_mutex> RunningLock;
    std::atomic<bool> Active;
};

} // namespace gc
} // namespace hydra

#endif // _THREAD_ALLOCATOR_H_