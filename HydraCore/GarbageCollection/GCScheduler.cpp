#include "GCScheduler.h"

#include "Heap.h"
#include "Region.h"

#include "Common/Logger.h"

namespace hydra
{
namespace gc
{

GCScheduler::EventHistory::EventHistory()
    : HistoryCounter(0)
{ }

void GCScheduler::EventHistory::Push(EventType type)
{
    Push(type, std::chrono::high_resolution_clock::now());
}

void GCScheduler::EventHistory::Push(EventType type, TimePoint time)
{
    Push(Event{ type, time });
}

void GCScheduler::EventHistory::Push(Event event)
{
    History[HistoryCounter++ % GC_SCHEDULER_HISTORY_SIZE] = event;
}

GCScheduler::EventHistory::iterator GCScheduler::EventHistory::begin()
{
    return iterator(this,
        HistoryCounter < GC_SCHEDULER_HISTORY_SIZE ?
            0 :
            HistoryCounter - GC_SCHEDULER_HISTORY_SIZE);
}

GCScheduler::EventHistory::iterator GCScheduler::EventHistory::end()
{
    return iterator(this, HistoryCounter);
}

GCScheduler::GCScheduler(Heap *owner)
    : Owner(owner),
    History(),
    RegionCountAfterLastYoungGC(0),
    RegionCountAfterLastFullGC(0),
    RegionCountBeforeFullGC(0),
    RegionCountLastMonitor(0),
    LastMonitor(std::chrono::high_resolution_clock::now()),
    FullGCStart()
{ }

void GCScheduler::OnFullGCStart()
{
    History.Push(EventType::E_FULL_GC_START);
    RegionCountBeforeLastFullGC = RegionCountBeforeFullGC;
    RegionCountBeforeFullGC = Owner->GetFullCleaningRegionCount();
    FullGCStart = std::chrono::high_resolution_clock::now();
}

void GCScheduler::OnFullGCEnd()
{
    History.Push(EventType::E_FULL_GC_END);
    RegionCountAfterLastFullGC = Owner->GetFullCleaningRegionCount();

    auto timeElapsed = std::chrono::duration_cast<Duration>(
            std::chrono::high_resolution_clock::now() - FullGCStart);
    double regionPerSecond = RegionCountBeforeFullGC / timeElapsed.count();

    UpdateValue(RegionProcessedInFullGCPerSecond, regionPerSecond);
}

void GCScheduler::OnYoungGCStart()
{
    History.Push(EventType::E_YOUNG_GC_START);
}

void GCScheduler::OnYoungGCEnd()
{
    History.Push(EventType::E_YOUNG_GC_END);
}

void GCScheduler::OnMonitor()
{
    auto regionCount = Owner->GetFullCleaningRegionCount();
    auto now = std::chrono::high_resolution_clock::now();

    if (RegionCountLastMonitor < regionCount)
    {
        auto timeElapsed = std::chrono::duration_cast<Duration>(now - LastMonitor);
        double regionPerSecond = (regionCount - RegionCountLastMonitor) / timeElapsed.count();

        UpdateValue(RegionOldFulledPerSecond, regionPerSecond);
    }

    RegionCountLastMonitor = regionCount;
    LastMonitor = now;
    return;
}

bool GCScheduler::ShouldYoungGC()
{
    size_t currentRegionCount = Owner->GetFullListRegionCount();
    size_t currentWorkingQueueLength = Owner->WorkingQueue.Count();

    return (currentWorkingQueueLength > Owner->WorkingQueue.Capacity() * YOUNG_GC_TRIGGER_FACTOR_BY_WORKING_QUEUE) ||
        (currentRegionCount > 0);
}

bool GCScheduler::ShouldFullGC()
{
    size_t currentRegionCount = Owner->GetFullCleaningRegionCount();

    if (!currentRegionCount)
    {
        return false;
    }

    if (currentRegionCount > MAXIMUM_REGION_COUNT)
    {
        return true;
    }

    double RegionCountToFullGCByIncrement = RegionCountAfterLastFullGC * FULL_GC_TRIGGER_FACTOR_BY_INCREMENT;
    double SecondsForAllocationByIncrement = (RegionCountToFullGCByIncrement - currentRegionCount) / RegionOldFulledPerSecond;

    double RegionCountToFullGCByLast = static_cast<double>(RegionCountBeforeLastFullGC);
    double SecondsForAllocationByLast = (RegionCountToFullGCByLast - currentRegionCount) / RegionOldFulledPerSecond;

    double SecondsForFullGC = currentRegionCount / RegionProcessedInFullGCPerSecond;

    bool ret = (SecondsForFullGC + GC_SCHEDULER_FULL_GC_ADVANCE_IN_SECOND >= SecondsForAllocationByIncrement) ||
        (SecondsForFullGC >= SecondsForAllocationByLast);

    if (ret)
    {
        Logger::GetInstance()->Log() << "Predict full gc: "
            << "[NextFullGCTime: " << (SecondsForFullGC * 1000) << "ms] "
            << "[AllocateToIncrement: " << (SecondsForAllocationByIncrement * 1000) << "ms] "
            << "[AllocateTolast: " << (SecondsForAllocationByLast * 1000) << "ms]";
    }

    return ret;
}

}
}