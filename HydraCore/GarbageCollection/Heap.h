#ifndef _HEAP_H_
#define _HEAP_H_

#include "Common/HydraCore.h"

#include "GCDefs.h"
#include "HeapObject.h"
#include "Region.h"
#include "Common/ConcurrentLinkedList.h"
#include "Common/ConcurrentQueue.h"
#include "Common/Singleton.h"
#include "Common/Logger.h"

#include <array>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <thread>

namespace hydra
{

namespace gc
{

class ThreadAllocator;

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
        GCCurrentPhase(GCPhase::GC_IDLE)
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
        Logger::GetInstance()->Log() << "Young GC requested";

        YoungGCRequested.store(true, std::memory_order_relaxed);
        ShouldGCCV.notify_one();
    }

    inline void RequestFullGC()
    {
        Logger::GetInstance()->Log() << "Full GC requested";

        FullGCRequested.store(true, std::memory_order_relaxed);
        ShouldGCCV.notify_one();
    }

    inline void Remember(HeapObject *obj)
    {
        if (obj)
        {
            u8 currentGCState = obj->GetGCState();

            if (currentGCState == GCState::GC_WHITE)
            {
                SetGCStateAndWorkingQueueEnqueue(obj);
            }
            else if (currentGCState == GCState::GC_DARK &&
                GCCurrentPhase.load() == GCPhase::GC_FULL_MARK)
            {
                SetGCStateAndWorkingQueueEnqueue(obj);
            }
        }
    }

    inline void SetGCStateAndWorkingQueueEnqueue(HeapObject *obj)
    {
        if (WorkingQueue.Count() > WorkingQueue.Capacity() * GC_WORKING_QUEUE_FACTOR)
        {
            RequestYoungGC();
        }

        auto originalGCState = obj->SetGCState(GCState::GC_GREY);
        if (originalGCState == GCState::GC_WHITE)
        {
            Region::GetRegionOfObject(obj)->IncreaseOldObjectCount();
            WorkingQueue.Enqueue(obj);
        }
        else if (originalGCState != GCState::GC_GREY)
        {
            // the originalGCState can be GC_BLACK, but we still add it to WorkingQueue
            WorkingQueue.Enqueue(obj);
        }
    }

    void WriteBarrier(HeapObject *target, HeapObject *ref);

    void StopTheWorld();
    void ResumeTheWorld();

    void Shutdown();

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
    concurrent::ForwardLinkedList<Region> FullList;
    concurrent::ForwardLinkedList<Region> CleaningList;
    concurrent::ForwardLinkedList<Region> FullCleaningList;

    concurrent::Queue<HeapObject*, 8192> WorkingQueue;
    std::atomic<size_t> GatheringWorkerCount;

    std::atomic<size_t> TotalThreads;
    std::atomic<size_t> ReportedThreads;

    // Stop-the-world
    std::atomic<bool> PauseRequested;
    std::shared_mutex RunningMutex;
    std::shared_mutex WaitingMutex;
    std::condition_variable_any WakeupCV;

    std::mutex ShouldGCMutex;
    std::condition_variable ShouldGCCV;
    std::atomic<bool> YoungGCRequested;
    std::atomic<bool> FullGCRequested;

    std::atomic<GCPhase> GCCurrentPhase;

    std::chrono::time_point<std::chrono::high_resolution_clock> WorldStopped;

    std::thread GCManagementThread;
    void GCManagement();
    void FireGCPhaseAndWait(GCPhase phase, bool cannotWait = false);

    size_t GCWorkerCount;
    void GCWorkerYoungMark();
    void GCWorkerYoungSweep();
    void GCWorkerFullMark();
    void GCWorkerFullSweep();

    bool AreAllWorkingThreadsReported();

    friend class ThreadAllocator;
};

} // namespace gc
} // namespace hydra

#endif // _HEAP_H_