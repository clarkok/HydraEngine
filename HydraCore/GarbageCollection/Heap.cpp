#include "Heap.h"

#include "Common/Logger.h"
#include "Common/ThreadPool.h"

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
            SetGCStateAndWorkingQueueEnqueue(target);
        }
    }
}

void Heap::StopTheWorld()
{
    if (!PauseRequested.exchange(true))
    {
        auto perfSession = Logger::GetInstance()->Perf("StoppingTheWorld");

        WaitingMutex.unlock();
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

        WaitingMutex.lock();
    }
}

void Heap::Shutdown()
{
    Logger::GetInstance()->Log() << "Heap shutdown requested";

    if (!ShouldExit.exchange(true))
    {
        GCManagementThread.join();
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

            Scheduler.OnMonitor();

            youngGCRequested = YoungGCRequested.exchange(false, std::memory_order_acq_rel);
            fullGCRequested = FullGCRequested.exchange(false, std::memory_order_acq_rel);

            youngGCRequested = youngGCRequested || Scheduler.ShouldYoungGC();
            fullGCRequested = fullGCRequested || Scheduler.ShouldFullGC();

            ReportedThreads.store(0, std::memory_order_relaxed);
        }

        while (!ShouldExit.load() && (youngGCRequested || fullGCRequested))
        {
            if (fullGCRequested)
            {
                auto perfSession = Logger::GetInstance()->Perf("FullGC");

                Logger::GetInstance()->Log() << "Before Full GC: "
                    << "[Total: " << Region::GetTotalRegionCount() << "] "
                    << "[YoungFull: " << GetYoungCleaningRegionCount() << "] "
                    << "[OldFull: " << GetFullCleaningRegionCount() << "]";

                Scheduler.OnFullGCStart();

                GCRound.fetch_add(1);
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

                FireGCPhaseAndWait(GCPhase::GC_FULL_SWEEP, [this]()
                {
                    std::unique_lock<std::shared_mutex> lck(LargeSetMutex);

                    for (auto it = LargeSet.begin(); it != LargeSet.end();)
                    {
                        auto gcState = (*it)->GetGCState();
                        if (gcState == GC_WHITE || gcState == GC_DARK)
                        {
                            auto obj = dynamic_cast<Cell*>(*it);
                            obj->~Cell();
                            std::free(obj);
                            it = LargeSet.erase(it);
                        }
                        else
                        {
                            while (gcState == GC_BLACK)
                            {
                                if ((*it)->TrySetGCState(gcState, GC_DARK))
                                {
                                    break;
                                }
                            }

                            ++it;
                        }
                    }
                });
                perfSession.Phase("Sweep");

                RegionSizeAfterLastFullGC.store(Region::GetTotalRegionCount(), std::memory_order_relaxed);

                Scheduler.OnFullGCEnd();

                Logger::GetInstance()->Log() << "After Full GC: "
                    << "[Total: " << Region::GetTotalRegionCount() << "] "
                    << "[YoungFull: " << GetYoungCleaningRegionCount() << "] "
                    << "[OldFull: " << GetFullCleaningRegionCount() << "]";
            }
            else if (youngGCRequested)
            {
                auto perfSession = Logger::GetInstance()->Perf("YoungGC");

                Logger::GetInstance()->Log() << "Before Young GC: "
                    << "[Total: " << Region::GetTotalRegionCount() << "] "
                    << "[YoungFull: " << GetYoungCleaningRegionCount() << "] "
                    << "[OldFull: " << GetFullCleaningRegionCount() << "]";

                Scheduler.OnYoungGCStart();

                GCRound.fetch_add(1);
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

                FireGCPhaseAndWait(GCPhase::GC_YOUNG_SWEEP, [this]()
                {
                    std::unique_lock<std::shared_mutex> lck(LargeSetMutex);

                    for (auto it = LargeSet.begin(); it != LargeSet.end();)
                    {
                        if ((*it)->GetGCState() == GC_WHITE)
                        {
                            auto obj = dynamic_cast<Cell*>(*it);
                            obj->~Cell();
                            std::free(obj);
                            it = LargeSet.erase(it);
                        }
                        else
                        {
                            ++it;
                        }
                    }
                });
                perfSession.Phase("Sweep");

                Scheduler.OnYoungGCEnd();

                Logger::GetInstance()->Log() << "After Young GC: "
                    << "[Total: " << Region::GetTotalRegionCount() << "] "
                    << "[YoungFull: " << GetYoungCleaningRegionCount() << "] "
                    << "[OldFull: " << GetFullCleaningRegionCount() << "]";
            }

            GCCurrentPhase.store(GCPhase::GC_IDLE);

            Scheduler.OnMonitor();

            youngGCRequested = YoungGCRequested.exchange(false, std::memory_order_acq_rel);
            fullGCRequested = FullGCRequested.exchange(false, std::memory_order_acq_rel);

            youngGCRequested = youngGCRequested || Scheduler.ShouldYoungGC();
            fullGCRequested = fullGCRequested || Scheduler.ShouldFullGC();

            ReportedThreads.store(0, std::memory_order_relaxed);
        }
    }

    GCCurrentPhase.store(GCPhase::GC_EXIT);

    Logger::GetInstance()->Log() << "GC Management shutdown";
}

void Heap::Fire(Heap::GCPhase phase, std::vector<std::future<void>> &futures)
{
    GCCurrentPhase.store(phase, std::memory_order_relaxed);
    std::generate(futures.begin(), futures.end(), [this, phase]()
    {
        auto threadPool = ThreadPool::GetInstance();

        switch (phase)
        {
        case GCPhase::GC_YOUNG_MARK:
        case GCPhase::GC_YOUNG_FINISH_MARK:
            return threadPool->Dispatch<void>(&Heap::GCWorkerYoungMark, this);
        case GCPhase::GC_YOUNG_SWEEP:
            return threadPool->Dispatch<void>(&Heap::GCWorkerYoungSweep, this);
        case GCPhase::GC_FULL_MARK:
        case GCPhase::GC_FULL_FINISH_MARK:
            return threadPool->Dispatch<void>(&Heap::GCWorkerFullMark, this);
        case GCPhase::GC_FULL_SWEEP:
            return threadPool->Dispatch<void>(&Heap::GCWorkerFullSweep, this);
        }
    });
}

void Heap::Wait(std::vector<std::future<void>> &futures, bool cannotWait)
{
    if (cannotWait)
    {
        auto stopped = ThreadPool::WaitAllFor(futures.begin(), futures.end(), GC_TOLERANCE);
        if (stopped == futures.end())
        {
            return;
        }

        StopTheWorld();

        ThreadPool::WaitAll(stopped, futures.end());
    }
    else
    {
        ThreadPool::WaitAll(futures.begin(), futures.end());
    }
}

void Heap::FireGCPhaseAndWait(Heap::GCPhase phase, bool cannotWait)
{
    std::vector<std::future<void>> futures(GCWorkerCount);
    Fire(phase, futures);
    Wait(futures, cannotWait);
}

void Heap::FireGCPhaseAndWait(Heap::GCPhase phase, std::function<void()> whenWaiting)
{
    std::vector<std::future<void>> futures(GCWorkerCount);
    Fire(phase, futures);
    whenWaiting();
    Wait(futures, false);
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
                    if (!ref->IsLarge())
                    {
                        Region::GetRegionOfObject(ref)->IncreaseOldObjectCount();
                    }
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
                if (!ref->IsLarge())
                {
                    Region::GetRegionOfObject(ref)->IncreaseOldObjectCount();
                }
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