#ifndef _HEAP_H_
#define _HEAP_H_

#include "Common/HydraCore.h"

#include "GCDefs.h"
#include "HeapObject.h"
#include "Region.h"
#include "GCScheduler.h"
#include "Common/ConcurrentLinkedList.h"
#include "Common/ConcurrentQueue.h"
#include "Common/Singleton.h"
#include "Common/Logger.h"

#include <array>
#include <vector>
#include <set>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <thread>
#include <future>

namespace hydra
{

namespace gc
{

class ThreadAllocator;
class GCScheduler;

//to add state(s), you must update Cell::GC_STATE_MASK accordingly in HeapObject.h
enum GCState : u8
{
    GC_WHITE = 0,
    GC_GREY = 1,
    GC_DARK = 2,
    GC_BLACK = 3
};

class Heap : public Singleton<Heap>
{
public:
    Region *GetFreeRegion(size_t level);
    void CommitFullRegion(Region * &region);

    Heap()
        : ShouldExit(false),
        GCRound(0),
        RegionSizeAfterLastFullGC(0),
        GatheringWorkerCount(0),
        TotalThreads(0),
        PauseRequested(false),
        YoungGCRequested(false),
        FullGCRequested(false),
        GCWorkerCount(std::min<size_t>(GC_WORKER_MAX_NR, std::thread::hardware_concurrency() / 2)),
        GCCurrentPhase(GCPhase::GC_IDLE),
        Scheduler(this)
    {
        WaitingMutex.lock();
        GCManagementThread = std::thread(&Heap::GCManagement, this);
    }

    ~Heap()
    {
        WaitingMutex.unlock();
        if (!ShouldExit.exchange(true))
        {
            GCManagementThread.join();
        }

        Region *region;
        for (size_t i = 0; i < LEVEL_NR; ++i)
        {
            while (FreeLists[i].Pop(region))
            {
                Region::Delete(region);
            }
        }

        while (FullList.Pop(region))
        {
            Region::Delete(region);
        }

        while (CleaningList.Pop(region))
        {
            Region::Delete(region);
        }

        while (FullCleaningList.Pop(region))
        {
            Region::Delete(region);
        }
    }

    inline void RequestYoungGC()
    {
        if (FullList.GetCount() == 0)
        {
            return;
        }

        Logger::GetInstance()->Log() << "Young GC requested";

        YoungGCRequested.store(true, std::memory_order_relaxed);
        ShouldGCCV.notify_one();
    }

    inline void RequestFullGC()
    {
        if (FullList.GetCount() == 0)
        {
            return;
        }

        Logger::GetInstance()->Log() << "Full GC requested";

        FullGCRequested.store(true, std::memory_order_relaxed);
        ShouldGCCV.notify_one();
    }

    inline void Remember(HeapObject *obj)
    {
        if (obj)
        {
            hydra_assert(obj->IsInUse(),
                "can only remember living objects");
            SetGCStateAndWorkingQueueEnqueue(obj);
        }
    }

    inline void SetGCStateAndWorkingQueueEnqueue(HeapObject *obj)
    {
        if (WorkingQueue.Count() > WorkingQueue.Capacity() * YOUNG_GC_TRIGGER_FACTOR_BY_WORKING_QUEUE)
        {
            RequestYoungGC();
        }

        auto originalGCState = obj->SetGCState(GCState::GC_GREY);
        if (originalGCState == GCState::GC_WHITE)
        {
            if (!obj->IsLarge())
            {
                Region::GetRegionOfObject(obj)->IncreaseOldObjectCount();
            }
            WorkingQueue.Enqueue(obj);
        }
        else if (originalGCState != GCState::GC_GREY)
        {
            // the originalGCState can be GC_BLACK, but we still add it to WorkingQueue
            WorkingQueue.Enqueue(obj);
        }
    }

    inline size_t GetFullListRegionCount()
    {
        return FullList.GetCount();
    }

    inline size_t GetYoungCleaningRegionCount()
    {
        return CleaningList.GetCount();
    }

    inline size_t GetFullCleaningRegionCount()
    {
        return FullCleaningList.GetCount();
    }

    inline void WriteBarrier(HeapObject *target, HeapObject *ref)
    {
        if (!ref)
        {
            return;
        }

        hydra_assert(target->IsInUse(), "target must be in use");
        hydra_assert(ref->IsInUse(), "ref must be in use");

        if (ref->GetGCState() == GCState::GC_WHITE)
        {
            u8 targetGCState = target->GetGCState();
            if (targetGCState == GCState::GC_DARK || targetGCState == GCState::GC_BLACK)
            {
                SetGCStateAndWorkingQueueEnqueue(target);
            }
            return;
        }

        auto phase = GCCurrentPhase.load();
        if (phase == GCPhase::GC_FULL_MARK && ref->GetGCState() == GCState::GC_DARK)
        {
            u8 targetGCState = target->GetGCState();
            if (targetGCState == GCState::GC_DARK || targetGCState == GCState::GC_BLACK)
            {
                SetGCStateAndWorkingQueueEnqueue(target);
            }
        }
    }

    static void WriteBarrierStatic(Heap *heap, HeapObject *target, HeapObject *ref);
    static void WriteBarrierInRegions(Heap *heap, HeapObject **target, HeapObject *ref);
    static void WriteBarrierIfInHeap(Heap *heap, void *target, HeapObject *ref);

    void StopTheWorld();
    void ResumeTheWorld();

    void Shutdown();

    inline bool IsLargeObject(void *ptr)
    {
        std::shared_lock<std::shared_mutex> lck(LargeSetMutex);
        return LargeSet.find(reinterpret_cast<HeapObject*>(ptr)) != LargeSet.end();
    }

    inline void RegisterLargeObject(HeapObject *ptr)
    {
        std::unique_lock<std::shared_mutex> lck(LargeSetMutex);
        LargeSet.insert(ptr);
    }

    inline void RegisterRootScanFunc(std::function<void(std::function<void(HeapObject *)>)> scanFunc)
    {
        std::unique_lock<std::mutex> lck(RootScanFuncMutex);

        RootScanFunc.push_back(scanFunc);
    }

    inline void FeedbackInactiveRegion(Region *region)
    {
        FreeLists[region->Level].Push(region);
    }

private:
    enum class GCPhase
    {
        GC_IDLE,

        GC_YOUNG_MARK,
        GC_YOUNG_FINISH_MARK,
        GC_YOUNG_SWEEP,

        GC_FULL_MARK,
        GC_FULL_FINISH_MARK,
        GC_FULL_SWEEP,

        GC_EXIT
    };

    std::atomic<bool> ShouldExit;
    std::atomic<size_t> GCRound;
    std::atomic<size_t> RegionSizeAfterLastFullGC;

    std::array<concurrent::ForwardLinkedList<Region>, LEVEL_NR> FreeLists;
    std::array<concurrent::ForwardLinkedList<Region>, LEVEL_NR> RemarkingLists;
    concurrent::ForwardLinkedList<Region> FullList;
    concurrent::ForwardLinkedList<Region> CleaningList;
    concurrent::ForwardLinkedList<Region> FullCleaningList;

    std::mutex RootScanFuncMutex;
    std::vector<std::function<void(std::function<void(HeapObject *)>)>> RootScanFunc;

    std::set<HeapObject *> LargeSet;
    std::shared_mutex LargeSetMutex;

    concurrent::Queue<HeapObject*, 8192> WorkingQueue;
    std::atomic<size_t> GatheringWorkerCount;

    std::atomic<size_t> TotalThreads;
    std::atomic<size_t> ReportedThreads;

    // Stop-the-world
    std::atomic<bool> PauseRequested;
    std::shared_mutex RunningMutex;
    std::shared_mutex WaitingMutex;
    std::condition_variable_any WakeupCV;
    std::atomic<size_t> WaitingThreadsCount;
    std::shared_mutex RemarkMutex;

    std::mutex ShouldGCMutex;
    std::condition_variable ShouldGCCV;
    std::atomic<bool> YoungGCRequested;
    std::atomic<bool> FullGCRequested;

    std::atomic<GCPhase> GCCurrentPhase;

    std::chrono::time_point<std::chrono::high_resolution_clock> WorldStopped;

    GCScheduler Scheduler;

    std::thread GCManagementThread;
    void GCManagement();
    void Fire(Heap::GCPhase phase, std::vector<std::future<void>> &futures);
    void Wait(std::vector<std::future<void>> &futures, bool cannotWait = false);
    void FireGCPhaseAndWait(GCPhase phase, bool cannotWait = false);
    void FireGCPhaseAndWait(GCPhase phase, std::function<void()> whenWaiting, bool cannotWait = false);

    size_t GCWorkerCount;
    void GCWorkerYoungMark();
    void GCWorkerYoungSweep();
    void GCWorkerFullMark();
    void GCWorkerFullSweep();

    bool AreAllWorkingThreadsReported();

    friend class ThreadAllocator;
    friend class GCScheduler;
};

} // namespace gc
} // namespace hydra

#endif // _HEAP_H_
