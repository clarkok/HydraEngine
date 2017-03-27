#include "Heap.h"

#include "Common/Logger.h"

#include <queue>

namespace hydra
{
namespace gc
{

Region *Heap::GetFreeRegion(size_t level)
{
    Region *ret = nullptr;
    FreeLists[level].Pop(ret);

    while (ret && ret->YoungSweep() == Region::CellCountFromLevel(level))
    {
        // wait for full gc
        CleaningLists[level].Push(ret);
        FreeLists[level].Pop(ret);
    }

    if (!ret)
    {
        ret = Region::New(level);
        RequestFullGC();
    }

    return ret;
}

void Heap::CommitFullRegion(Region *&region)
{
    auto level = region->Level;

    FullLists[level].Push(region);
    region = GetFreeRegion(level);

    RequestYoungGC();
}

void Heap::WriteBarrier(HeapObject *target, HeapObject *ref)
{
    if (ref && ref->GetGCState() == GCState::GC_WHITE)
    {
        u8 targetGCState = target->GetGCState();
        if (targetGCState == GCState::GC_DARK || targetGCState == GCState::GC_BLACK)
        {
            if (target->SetGCState(GCState::GC_GREY) != GCState::GC_GREY)
            {
                WorkingQueueEnqueue(target);
            }
        }
    }
}

void Heap::StopTheWorld()
{
    if (!PauseRequested.exchange(true))
    {
        RunningMutex.lock();
        Logger::GetInstance()->Log() << "Stop the world";
    }
}

void Heap::ResumeTheWorld()
{
    if (PauseRequested.exchange(false))
    {
        Logger::GetInstance()->Log() << "Resume the world";
        RunningMutex.unlock();
        WakeupCV.notify_all();
    }
}

void Heap::GCManagement()
{
    bool youngGCRequested = false;
    bool fullGCRequested = false;

    while (!ShouldExit.load())
    {
        {
            std::unique_lock<std::mutex> lck(ShouldGCMutex);
            ShouldGCCV.wait_for(lck, GC_CHECK_INTERVAL);

            youngGCRequested = YoungGCRequested.exchange(false, std::memory_order_acq_rel);
            fullGCRequested = FullGCRequested.exchange(false, std::memory_order_acq_rel);
            ReportedThreads.store(0, std::memory_order_relaxed);
        }

        if (fullGCRequested)
        {
            Logger::GetInstance()->Log() << "Full GC started";

            GCRount.fetch_add(1, std::memory_order_acq_rel);
            FireGCPhaseAndWait(GCPhase::GC_FULL_MARK);

            StopTheWorld();
            FireGCPhaseAndWait(GCPhase::GC_FULL_FINISH_MARK);

            for (size_t level = 0; level < LEVEL_NR; ++level)
            {
                CleaningLists[level].Steal(FullLists[level]);
            }
            ResumeTheWorld();

            FireGCPhaseAndWait(GCPhase::GC_FULL_SWEEP);

            Logger::GetInstance()->Log() << "Full GC finished";
        }
        else if (youngGCRequested)
        {
            Logger::GetInstance()->Log() << "Young GC started";
            auto gcStart = std::chrono::high_resolution_clock::now();

            GCRount.fetch_add(1, std::memory_order_acq_rel);
            FireGCPhaseAndWait(GCPhase::GC_YOUNG_MARK);

            Logger::GetInstance()->Log() << "Young GC mark 1";

            StopTheWorld();
            FireGCPhaseAndWait(GCPhase::GC_YOUNG_FINISH_MARK);

            hydra_assert(WorkingQueue.Count() == 0,
                "WorkingQueue should be empty now");

            Logger::GetInstance()->Log() << "Young GC mark 2";

            for (size_t level = 0; level < LEVEL_NR; ++level)
            {
                FreeLists[level].Steal(FullLists[level]);
            }

            ResumeTheWorld();

            auto gcEnd = std::chrono::high_resolution_clock::now();
            Logger::GetInstance()->Log() << "Young GC finished: " << (gcEnd - gcStart).count() / 1000000.0 << "ms";
        }

        GCCurrentPhase.store(GCPhase::GC_IDLE);
    }

    GCCurrentPhase.store(GCPhase::GC_EXIT);
    GCWorkerRunningCV.notify_all();
}

void Heap::FireGCPhaseAndWait(Heap::GCPhase phase)
{
    std::unique_lock<std::shared_mutex> lck(GCWorkerRunningMutex);

    hydra_assert(GCWorkerCompletedCount.load() == GCWorkerCount,
        "All GCWorker should be waiting");

    GCWorkerCompletedCount.store(0, std::memory_order_release);
    GCCurrentPhase.store(phase, std::memory_order_relaxed);

    GCWorkerRunningCV.notify_all();
    GCWorkerCompletedCV.wait(
        lck,
        [this]()
        {
            return GCWorkerCompletedCount.load(std::memory_order_consume) == GCWorkerCount;
        });
}

void Heap::GCWorker()
{
    GCPhase phase = GCPhase::GC_IDLE;
    GCWorkerCompletedCount.fetch_add(1);

    while (!ShouldExit.load())
    {
        std::shared_lock<std::shared_mutex> lck(GCWorkerRunningMutex);
        GCPhase currentPhase = GCCurrentPhase.load();
        if (phase != currentPhase)
        {
            phase = currentPhase;
        }
        else
        {
            GCWorkerRunningCV.wait(lck);

            phase = GCCurrentPhase.load(std::memory_order_acquire);
        }

        switch (phase)
        {
        case GCPhase::GC_YOUNG_MARK:
        case GCPhase::GC_YOUNG_FINISH_MARK:
            GCWorkerYoungMark();
            break;
        case GCPhase::GC_FULL_MARK:
        case GCPhase::GC_FULL_FINISH_MARK:
            GCWorkerFullMark();
            break;
        case GCPhase::GC_FULL_SWEEP:
            GCWorkerFullSweep();
            break;
        case GCPhase::GC_IDLE:
            continue;
        case GCPhase::GC_EXIT:
            return;
        default:
            trap("Unknown GC phase: " + std::to_string(static_cast<int>(phase)));
        }

        lck.unlock();
        if (GCWorkerCompletedCount.fetch_add(1, std::memory_order_relaxed) + 1 == GCWorkerCount)
        {
            GCWorkerCompletedCV.notify_one();
        }
    }
}

void Heap::GCWorkerYoungMark()
{
    std::queue<HeapObject *> localQueue;
    bool shouldWaitForWorkingThreadReported = GCCurrentPhase.load() == GCPhase::GC_YOUNG_MARK;

    for (;;)
    {
        // gather from global working queue
        size_t workingQueueSize = WorkingQueue.Count();
        if (
            workingQueueSize == 0 &&
            localQueue.empty() &&
            (!shouldWaitForWorkingThreadReported || AreAllWorkingThreadsReported()))
        {
            break;
        }

        size_t gatheringWorkerCount = GatheringWorkerCount.fetch_add(1);
        size_t gatheringObjectCount = (workingQueueSize + gatheringWorkerCount) / (gatheringWorkerCount + 1);

        while (gatheringObjectCount--)
        {
            HeapObject *obj;
            if (!WorkingQueue.TryDequeue(obj))
            {
                break;
            }
            localQueue.push(obj);
        }

        GatheringWorkerCount.fetch_add(-1);

        // process local queue
        while (!localQueue.empty())
        {
            size_t objectsToProccessed = GC_WORKER_BALANCE_FACTOR;
            while (!localQueue.empty() && objectsToProccessed--)
            {
                HeapObject *obj = localQueue.front(); localQueue.pop();
                hydra_assert(obj, "obj should not be nullptr");

                if (obj->SetGCState(GCState::GC_DARK) == GCState::GC_DARK)
                {
                    continue;
                }

                obj->Scan([&](HeapObject *ref)
                {
                    hydra_assert(ref, "Reference should not be nullptr");

                    u8 gcState = ref->GetGCState();

                    if (gcState == GCState::GC_WHITE)
                    {
                        while (!ref->TrySetGCState(gcState, GCState::GC_GREY))
                        {
                            if (gcState != GCState::GC_WHITE)
                            {
                                break;
                            }
                        }

                        if (gcState == GCState::GC_WHITE)
                        {
                            Region::GetRegionOfObject(ref)->IncreaseOldObjectCount();
                            localQueue.push(ref);
                        }
                    }
                });
            }

            // feed back half of localQueue to global WorkingQueue
            size_t objectsToFeedBack = localQueue.size() / 2;
            while (objectsToFeedBack--)
            {
                HeapObject *obj = localQueue.front(); localQueue.pop();
                if (!WorkingQueue.TryEnqueue(obj))
                {
                    localQueue.push(obj);
                    break;
                }
            }
        }
    }
}

void Heap::GCWorkerFullMark()
{
    std::queue<HeapObject *> localQueue;
    bool shouldWaitForWorkingThreadReported = GCCurrentPhase.load() == GCPhase::GC_FULL_MARK;

    for (;;)
    {
        // gather from global working queue
        size_t workingQueueSize = WorkingQueue.Count();
        if (
            workingQueueSize == 0 &&
            localQueue.empty() &&
            (!shouldWaitForWorkingThreadReported || AreAllWorkingThreadsReported()))
        {
            break;
        }

        size_t gatheringWorkerCount = GatheringWorkerCount.fetch_add(1);
        size_t gatheringObjectCount = (workingQueueSize + gatheringWorkerCount) / (gatheringWorkerCount + 1);

        while (gatheringObjectCount--)
        {
            HeapObject *obj;
            if (!WorkingQueue.TryDequeue(obj))
            {
                break;
            }
            localQueue.push(obj);
        }

        GatheringWorkerCount.fetch_add(-1);

        // process local queue
        while (!localQueue.empty())
        {
            size_t objectsToProccessed = GC_WORKER_BALANCE_FACTOR;
            while (!localQueue.empty() && objectsToProccessed--)
            {
                HeapObject *obj = localQueue.front(); localQueue.pop();

                if (obj->SetGCState(GCState::GC_BLACK) == GCState::GC_BLACK)
                {
                    continue;
                }

                obj->Scan([&](HeapObject *ref)
                {
                    u8 gcState = ref->GetGCState();

                    if (gcState == GCState::GC_WHITE)
                    {
                        while (!ref->TrySetGCState(gcState, GCState::GC_GREY))
                        {
                            if (gcState != GCState::GC_WHITE)
                            {
                                break;
                            }
                        }

                        if (gcState == GCState::GC_WHITE)
                        {
                            Region::GetRegionOfObject(ref)->IncreaseOldObjectCount();
                            localQueue.push(ref);
                        }
                    }
                    else if (gcState == GCState::GC_DARK)
                    {
                        if (ref->SetGCState(GCState::GC_GREY) == GCState::GC_DARK)
                        {
                            localQueue.push(ref);
                        }
                    }
                });
            }

            // feed back half of localQueue to global WorkingQueue
            size_t objectsToFeedBack = localQueue.size() / 2;
            while (objectsToFeedBack--)
            {
                HeapObject *obj = localQueue.front(); localQueue.pop();
                if (!WorkingQueue.TryEnqueue(obj))
                {
                    localQueue.push(obj);
                    break;
                }
            }
        }
    }
}

void Heap::GCWorkerFullSweep()
{
    for (size_t level = 0; level < LEVEL_NR; ++level)
    {
        Region *region;
        while (CleaningLists[level].Pop(region))
        {
            if (region->FullSweep() == 0)
            {
                Region::Delete(region);
            }
            else
            {
                FreeLists[level].Push(region);
            }
        }
    }
}

bool Heap::AreAllWorkingThreadsReported()
{
    return ReportedThreads.load() == TotalThreads.load();
}

} // namespace gc
} // namespace hydra