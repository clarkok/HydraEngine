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
        GCRount(0),
        RegionSizeAfterLastFullGC(0),
        GatheringWorkerCount(0),
        TotalThreads(0),
        PauseRequested(false),
        YoungGCRequested(false),
        FullGCRequested(false),
        GCWorkerCount(std::min<size_t>(GC_WORKER_MAX_NR, std::thread::hardware_concurrency() / 2)),
        GCCurrentPhase(GCPhase::GC_IDLE),
        GCWorkerCompletedCount(0)
    {
        GCManagementThread = std::thread(&Heap::GCManagement, this);

        for (size_t i = 0; i < GCWorkerCount; ++i)
        {
            GCWorkerThreads.emplace_back(&Heap::GCWorker, this);
        }
    }

    ~Heap()
    {
        if (!ShouldExit.exchange(true))
        {
            GCManagementThread.join();

            for (auto &worker : GCWorkerThreads)
            {
                worker.join();
            }
        }

        for (size_t i = 0; i < LEVEL_NR; ++i)
        {
            Region *region;
            while (FreeLists[i].Pop(region))
            {
                Region::Delete(region);
            }

            while (FullLists[i].Pop(region))
            {
                Region::Delete(region);
            }

            while (CleaningLists[i].Pop(region))
            {
                Region::Delete(region);
            }
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
            auto currentGCPhase = GCCurrentPhase.load();
            u8 currentGCState = obj->GetGCState();

            switch (currentGCState)
            {
            case GCState::GC_WHITE:
                if (obj->SetGCState(GCState::GC_GREY) == GCState::GC_WHITE)
                {
                    Region::GetRegionOfObject(obj)->IncreaseOldObjectCount();
                    WorkingQueueEnqueue(obj);
                }
                break;
            case GCState::GC_DARK:
                if (currentGCPhase == GCPhase::GC_FULL_MARK &&
                    obj->SetGCState(GCState::GC_GREY) == GCState::GC_DARK)
                {
                    WorkingQueueEnqueue(obj);
                }
                break;
            }
        }
    }

    inline void WorkingQueueEnqueue(HeapObject *obj)
    {
        if (WorkingQueue.Count() > WorkingQueue.Capacoty() * GC_WORKING_QUEUE_FACTOR)
        {
            RequestYoungGC();
        }
        WorkingQueue.Enqueue(obj);
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

        GC_FULL_MARK,
        GC_FULL_FINISH_MARK,
        GC_FULL_SWEEP,

        GC_EXIT
    };

    std::atomic<bool> ShouldExit;
    std::atomic<size_t> GCRount;
    std::atomic<size_t> RegionSizeAfterLastFullGC;

    std::array<concurrent::ForwardLinkedList<Region>, LEVEL_NR> FreeLists;
    std::array<concurrent::ForwardLinkedList<Region>, LEVEL_NR> FullLists;
    std::array<concurrent::ForwardLinkedList<Region>, LEVEL_NR> CleaningLists;

    concurrent::Queue<HeapObject*> WorkingQueue;
    std::atomic<size_t> GatheringWorkerCount;

    std::atomic<size_t> TotalThreads;
    std::atomic<size_t> ReportedThreads;

    std::atomic<bool> PauseRequested;
    std::shared_mutex RunningMutex;
    std::condition_variable_any WakeupCV;

    std::mutex ShouldGCMutex;
    std::condition_variable ShouldGCCV;
    std::atomic<bool> YoungGCRequested;
    std::atomic<bool> FullGCRequested;

    std::atomic<size_t> GCWorkerCompletedCount;
    std::shared_mutex GCWorkerRunningMutex;
    std::condition_variable_any GCWorkerRunningCV;
    std::condition_variable_any GCWorkerCompletedCV;
    std::atomic<GCPhase> GCCurrentPhase;

    std::thread GCManagementThread;
    void GCManagement();
    void FireGCPhaseAndWait(GCPhase phase);

    size_t GCWorkerCount;
    std::vector<std::thread> GCWorkerThreads;
    void GCWorker();
    void GCWorkerYoungMark();
    void GCWorkerFullMark();
    void GCWorkerFullSweep();

    bool AreAllWorkingThreadsReported();

    friend class ThreadAllocator;
};

} // namespace gc
} // namespace hydra

#endif // _HEAP_H_