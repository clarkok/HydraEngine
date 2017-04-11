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

    if (!ret)
    {
        RequestYoungGC();
        ret = Region::New(level);
    }
    else if (FreeLists[level].GetCount() < 2)
    {
        RequestYoungGC();
    }

    hydra_assert(ret->Level == level,
        "Level of region should match to expected");

    if (GCCurrentPhase.load(std::memory_order_relaxed) == GCPhase::GC_IDLE)
    {
        size_t currentRegionCount = Region::GetTotalRegionCount();

        if (currentRegionCount >
            RegionSizeAfterLastFullGC.load(std::memory_order_relaxed) * FULL_GC_TRIGGER_FACTOR_BY_INCREMENT)
        {
            RequestFullGC();
        }
        else if (currentRegionCount > MAXIMUM_REGION_COUNT * FULL_GC_TRIGGER_FACTOR_BY_HEAP_SIZE)
        {
            RequestFullGC();
        }
    }

    return ret;
}

void Heap::CommitFullRegion(Region *&region)
{
    auto level = region->Level;

    FullList.Push(region);
    region = GetFreeRegion(level);
}

void Heap::WriteBarrier(HeapObject *target, HeapObject *ref)
{
    if (ref && ref->GetGCState() == GCState::GC_WHITE)
    {
        u8 targetGCState = target->GetGCState();
        if (targetGCState == GCState::GC_DARK || targetGCState == GCState::GC_BLACK)
        {
            auto gcPhase = GCCurrentPhase.load();
            if (gcPhase == GCPhase::GC_FULL_SWEEP || gcPhase == GCPhase::GC_YOUNG_SWEEP)
            {
                SetGCStateAndWorkingQueueEnqueue(ref);
            }
            else
            {
                SetGCStateAndWorkingQueueEnqueue(target);
            }
        }
    }
}

void Heap::StopTheWorld()
{
    if (!PauseRequested.exchange(true))
    {
        auto perfSession = Logger::GetInstance()->Perf("StoppingTheWorld");

        RunningMutex.lock();
        Logger::GetInstance()->Log() << "Stop the world";

        WorldStopped = std::chrono::high_resolution_clock::now();
    }
}

void Heap::ResumeTheWorld()
{
    if (PauseRequested.exchange(false))
    {
        auto perfSession = Logger::GetInstance()->Perf("ResumingTheWorld");

        auto now = std::chrono::high_resolution_clock::now();
        Logger::GetInstance()->Log() << "Resume the world " <<
            std::chrono::duration_cast<std::chrono::microseconds>(now - WorldStopped).count() / 1000. << "ms";
        RunningMutex.unlock();
        WakeupCV.notify_all();
    }
}

void Heap::Shutdown()
{
    Logger::GetInstance()->Log() << "Heap shutdown requested";

    if (!ShouldExit.exchange(true))
    {
        GCManagementThread.join();
        for (auto &worker : GCWorkerThreads)
        {
            worker.join();
        }
    }

    Logger::GetInstance()->Log() << "Heap shutdown";
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
            if (WorkingQueue.Count() > WorkingQueue.Capacity() * GC_WORKING_QUEUE_FACTOR)
            {
                youngGCRequested = true;
            }

            ReportedThreads.store(0, std::memory_order_relaxed);
        }

        while (!ShouldExit.load() && (youngGCRequested || fullGCRequested))
        {
            if (fullGCRequested)
            {
                Logger::GetInstance()->Log() << "Before Full GC: " << Region::GetTotalRegionCount() << " Regions";

                auto perfSession = Logger::GetInstance()->Perf("FullGC");

                GCRound.fetch_add(1, std::memory_order_acq_rel);
                FireGCPhaseAndWait(GCPhase::GC_FULL_MARK, true /* cannotWait */);
                perfSession.Phase("InitialMark");

                StopTheWorld();
                perfSession.Phase("AfterStopTheWorld");

                FireGCPhaseAndWait(GCPhase::GC_FULL_FINISH_MARK);
                hydra_assert(WorkingQueue.Count() == 0,
                    "WorkingQueue should be empty now");
                perfSession.Phase("FinishMark");

                FullCleaningList.Steal(FullList);
                perfSession.Phase("BeforeResumeTheWorld");
                ResumeTheWorld();

                FireGCPhaseAndWait(GCPhase::GC_FULL_SWEEP);
                perfSession.Phase("Sweep");

                RegionSizeAfterLastFullGC.store(Region::GetTotalRegionCount(), std::memory_order_relaxed);

                Logger::GetInstance()->Log() << "After Full GC: " << Region::GetTotalRegionCount() << " Regions";
            }
            else if (youngGCRequested)
            {
                auto perfSession = Logger::GetInstance()->Perf("YoungGC");

                GCRound.fetch_add(1, std::memory_order_acq_rel);
                FireGCPhaseAndWait(GCPhase::GC_YOUNG_MARK, true /* cannotWait */);
                perfSession.Phase("InitialMark");

                StopTheWorld();
                perfSession.Phase("AfterStopTheWorld");

                FireGCPhaseAndWait(GCPhase::GC_YOUNG_FINISH_MARK);
                hydra_assert(WorkingQueue.Count() == 0,
                    "WorkingQueue should be empty now");
                perfSession.Phase("FinishMark");

                CleaningList.Steal(FullList);
                perfSession.Phase("BeforeResumeTheWorld");
                ResumeTheWorld();

                FireGCPhaseAndWait(GCPhase::GC_YOUNG_SWEEP);
                perfSession.Phase("Sweep");
            }

            GCCurrentPhase.store(GCPhase::GC_IDLE);

            youngGCRequested = YoungGCRequested.exchange(false, std::memory_order_acq_rel);
            fullGCRequested = FullGCRequested.exchange(false, std::memory_order_acq_rel);
            if (WorkingQueue.Count() > WorkingQueue.Capacity() * GC_WORKING_QUEUE_FACTOR)
            {
                youngGCRequested = true;
            }

            ReportedThreads.store(0, std::memory_order_relaxed);
        }
    }

    GCCurrentPhase.store(GCPhase::GC_EXIT);
    GCWorkerRunningCV.notify_all();

    Logger::GetInstance()->Log() << "GC Management shutdown";
}

void Heap::FireGCPhaseAndWait(Heap::GCPhase phase, bool cannotWait)
{
    std::unique_lock<std::shared_mutex> lck(GCWorkerRunningMutex);

    hydra_assert(GCWorkerCompletedCount.load() == GCWorkerCount,
        "All GCWorker should be waiting");

    GCWorkerCompletedCount.store(0, std::memory_order_release);
    GCCurrentPhase.store(phase, std::memory_order_relaxed);

    GCWorkerRunningCV.notify_all();

    if (cannotWait)
    {
        GCWorkerCompletedCV.wait_for(
            lck,
            GC_TOLERANCE,
            [this]()
            {
                return GCWorkerCompletedCount.load(std::memory_order_consume) == GCWorkerCount;
            }
        );

        StopTheWorld();

        if (GCWorkerCompletedCount.load(std::memory_order_consume) != GCWorkerCount)
        {
            GCWorkerCompletedCV.wait(
                lck,
                [this]()
                {
                    return GCWorkerCompletedCount.load(std::memory_order_consume) == GCWorkerCount;
                }
            );
        }
    }
    else
    {
        GCWorkerCompletedCV.wait(
            lck,
            [this]()
            {
                return GCWorkerCompletedCount.load(std::memory_order_consume) == GCWorkerCount;
            }
        );
    }
}

void Heap::GCWorker()
{
    GCPhase phase = GCPhase::GC_IDLE;
    GCWorkerCompletedCount.fetch_add(1);

    while (true)
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
        case GCPhase::GC_YOUNG_SWEEP:
            GCWorkerYoungSweep();
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
    auto youngMarkPerf = Logger::GetInstance()->Perf("YoungMark");

    std::queue<HeapObject *> localQueue;
    bool shouldWaitForWorkingThreadReported = GCCurrentPhase.load() == GCPhase::GC_YOUNG_MARK;
    std::function<void(HeapObject *)> scanner = [&](HeapObject *ref)
    {
        hydra_assert(ref, "Reference should not be nullptr");

        u8 gcState = ref->GetGCState();

        if (gcState == GCState::GC_WHITE)
        {
            while (gcState == GCState::GC_WHITE)
            {
                if (ref->TrySetGCState(gcState, GCState::GC_GREY))
                {
                    Region::GetRegionOfObject(ref)->IncreaseOldObjectCount();
                    localQueue.push(ref);
                    break;
                }
            }
        }
    };

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

                hydra_assert(scanner.operator bool(), "Scanner should be valid");
                obj->Scan(scanner);
            }

            // feed back half of localQueue to global WorkingQueue
            size_t objectsToFeedBack = localQueue.size() / 2;
            Logger::GetInstance()->Log() << "Feed " << objectsToFeedBack << " back to global queue (" << WorkingQueue.Count() << ")";
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

void Heap::GCWorkerYoungSweep()
{
    auto youngSweepPerf = Logger::GetInstance()->Perf("YoungSweep");

    Region *region;
    while (CleaningList.Pop(region))
    {
        if (region->YoungSweep() == Region::CellCountFromLevel(region->Level))
        {
            FullCleaningList.Push(region);
        }
        else
        {
            FreeLists[region->Level].Push(region);
        }
    }
}

void Heap::GCWorkerFullMark()
{
    auto fullMarkPerf = Logger::GetInstance()->Perf("FullMark");

    std::queue<HeapObject *> localQueue;
    bool shouldWaitForWorkingThreadReported = GCCurrentPhase.load() == GCPhase::GC_FULL_MARK;
    std::function<void(HeapObject *)> scanner = [&](HeapObject *ref)
    {
        u8 gcState = ref->GetGCState();

        if (gcState == GCState::GC_WHITE || gcState == GCState::GC_DARK)
        {
            auto originalGCState = ref->SetGCState(GCState::GC_GREY);
            if (originalGCState == GCState::GC_WHITE)
            {
                Region::GetRegionOfObject(ref)->IncreaseOldObjectCount();
                localQueue.push(ref);
            }
            else if (originalGCState != GCState::GC_GREY)
            {
                localQueue.push(ref);
            }
        }
    };

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

                obj->Scan(scanner);
            }

            // feed back half of localQueue to global WorkingQueue
            size_t objectsToFeedBack = localQueue.size() / 2;
            Logger::GetInstance()->Log() << "Feed " << objectsToFeedBack << " back to global queue (" << WorkingQueue.Count() << ")";
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
    auto fullSweepPerf = Logger::GetInstance()->Perf("FullSweep");

    Region *region;
    while (FullCleaningList.Pop(region))
    {
        if (region->FullSweep() == 0)
        {
            Region::Delete(region);
        }
        else
        {
            FreeLists[region->Level].Push(region);
        }
    }
}

bool Heap::AreAllWorkingThreadsReported()
{
    return ReportedThreads.load() >= TotalThreads.load();
}

} // namespace gc
} // namespace hydra