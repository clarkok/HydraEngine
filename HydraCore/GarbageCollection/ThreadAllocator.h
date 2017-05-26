#ifndef _THREAD_ALLOCATOR_H_
#define _THREAD_ALLOCATOR_H_

#include "Common/HydraCore.h"

#include "Heap.h"
#include "Region.h"
#include "GCDefs.h"

#include "Common/Platform.h"
#include "Common/AutoCounter.h"
#include "Heap.h"

#include <algorithm>
#include <array>

namespace hydra
{

class ThreadAllocatorTester;

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
        if (Active.exchange(false))
        {
            Owner->TotalThreads.fetch_add(-1, std::memory_order_relaxed);
            RunningLock.unlock();
        }
    }

    template <typename T, typename T_Report, typename ...T_Args>
    T* Allocate(T_Report reportFunc, T_Args ...args)
    {
        return AllocateWithSize<T>(sizeof(T), reportFunc, args...);
    }

    template <typename T, typename ...T_Args>
    T* AllocateAuto(T_Args ...args)
    {
        return Allocate<T>(ThreadScan, args...);
    }

    template <typename T, typename T_Report, typename ...T_Args>
    T* AllocateWithSize(size_t size, T_Report reportFunc, T_Args ...args)
    {
        static_assert(std::is_base_of<HeapObject, T>::value,
            "T should inhert from HeapObject");

        Checkpoint(reportFunc);

        if (size > MAXIMAL_ALLOCATE_SIZE)
        {
            void *ptr = malloc(size);
            hydra_assert(ptr, "failed to allocate memory");

            T *allocated = new (ptr) T(HeapObject::IS_LARGE, args...);
            Owner->RegisterLargeObject(allocated);

            return allocated;
        }

        size_t level = Region::GetLevelFromSize(size);
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

    template <typename T, typename ...T_Args>
    T* AllocateWithSizeAuto(size_t size, T_Args ...args)
    {
        return AllocateWithSize<T>(size, ThreadScan, args...);
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

    template <typename T_Report>
    inline void Checkpoint(T_Report reportFunc)
    {
        size_t currentGCRound = Owner->GCRound.load();
        auto currentGCPhase = Owner->GCCurrentPhase.load();
        if (ReportedGCRound != currentGCRound &&
            (currentGCPhase == Heap::GCPhase::GC_YOUNG_MARK ||
             currentGCPhase == Heap::GCPhase::GC_FULL_MARK))
        {
            ReportedGCRound = currentGCRound;
            reportFunc();
            Owner->ReportedThreads.fetch_add(1);
        }
        else if (Owner->PauseRequested.load())
        {
            auto checkpointPerf = Logger::GetInstance()->Perf("CheckpointPauseRequest");

            reportFunc();
            currentGCRound = Owner->GCRound.load();
            if (currentGCRound != ReportedGCRound)
            {
                ReportedGCRound = currentGCRound;
                Owner->ReportedThreads.fetch_add(1);
            }

            checkpointPerf.Phase("Report");
            std::shared_lock<std::shared_mutex> remarkingLock(Owner->RemarkMutex);

            {
                std::shared_lock<std::shared_mutex> waitingLock(Owner->WaitingMutex);
                AutoCounter<size_t> autoWaitingThreadCount(Owner->WaitingThreadsCount);
                Owner->WakeupCV.wait(RunningLock,
                    [this]() { return !Owner->PauseRequested.load(); });
            }

            checkpointPerf.Phase("Wakeup");

            if (currentGCPhase == Heap::GCPhase::GC_FULL_MARK ||
                currentGCPhase == Heap::GCPhase::GC_FULL_FINISH_MARK ||
                currentGCPhase == Heap::GCPhase::GC_FULL_SWEEP)
            {
                for (auto &region : LocalPool)
                {
                    if (region)
                    {
                        region->RemarkBlockObject();
                    }
                }

                checkpointPerf.Phase("Remark");
            }
        }
    }

private:
    Heap *Owner;
    std::array<Region *, LEVEL_NR> LocalPool;
    size_t ReportedGCRound;
    std::shared_lock<std::shared_mutex> RunningLock;
    std::atomic<bool> Active;

    static void ThreadScan();

    friend class hydra::ThreadAllocatorTester;
};

} // namespace gc
} // namespace hydra

#endif // _THREAD_ALLOCATOR_H_