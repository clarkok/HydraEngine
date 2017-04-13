#ifndef _GC_SCHEDULER_H_
#define _GC_SCHEDULER_H_

#include "Common/HydraCore.h"
#include "GCDefs.h"

#include <chrono>
#include <array>
#include <iterator>

namespace hydra
{
namespace gc
{

class Heap;

class GCScheduler
{
public:
    GCScheduler(Heap *owner);

    void OnFullGCStart();
    void OnFullGCEnd();
    void OnYoungGCStart();
    void OnYoungGCEnd();
    void OnMonitor();

    bool ShouldYoungGC();
    bool ShouldFullGC();

private:
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;
    using Duration = std::chrono::duration<double, std::chrono::seconds::period>;

    enum class EventType
    {
        E_FULL_GC_START,
        E_FULL_GC_END,
        E_YOUNG_GC_START,
        E_YOUNG_GC_END
    };

    struct Event
    {
        EventType Type;
        TimePoint Time;
    };

    struct EventHistory
    {
    public:
        struct iterator : public std::iterator<std::random_access_iterator_tag, Event>
        {
            iterator(EventHistory *owner = nullptr, size_t offset = 0)
                : Owner(owner), Offset(offset)
            { }
            iterator(const iterator &) = default;
            iterator(iterator &&) = default;
            ~iterator() = default;

            iterator &operator = (const iterator &) = default;
            iterator &operator = (iterator &&) = default;

            inline bool operator == (iterator other) const
            {
                return Owner == other.Owner &&
                    Offset == other.Offset;
            }

            inline bool operator != (iterator other) const
            {
                return !operator == (other);
            }

            inline bool Valid() const
            {
                // this covers the situation that Owner->HistoryCounter < GC_SCHEDULER_HISTORY_SIZE
                return Owner &&
                    Offset >= Owner->HistoryCounter - GC_SCHEDULER_HISTORY_SIZE &&
                    Offset < Owner->HistoryCounter;
            }

            Event &operator *()
            {
                hydra_assert(Valid(), "Can only dereference a valid iterator");
                return Owner->History[Offset];
            }

            Event *operator ->()
            {
                hydra_assert(Valid(), "Can only dereference a valid interator");
                return &Owner->History[Offset];
            }

            iterator & operator ++()
            {
                if (Valid())
                {
                    Offset ++;
                }
                return *this;
            }

            iterator operator ++(int)
            {
                auto ret = *this;
                operator++();
                return ret;
            }

            iterator & operator --()
            {
                if (Valid())
                {
                    Offset--;
                }
                return *this;
            }

            iterator operator --(int)
            {
                auto ret = *this;
                operator--();
                return ret;
            }

        private:
            EventHistory *Owner;
            size_t Offset;
        };

        EventHistory();

        void Push(EventType type);
        void Push(EventType type, TimePoint time);
        void Push(Event event);

        iterator begin();
        iterator end();

    private:
        std::array<Event, GC_SCHEDULER_HISTORY_SIZE> History;
        size_t HistoryCounter;
    };

    inline double UpdateValue(double &value, double update)
    {
        double newValue =
            (value * (1 - GC_SCHEDULER_UPDATE_FACTOR)) +
             update * (GC_SCHEDULER_UPDATE_FACTOR);

        return value = newValue;
    }

    Heap *Owner;
    EventHistory History;
    size_t RegionCountAfterLastYoungGC;
    size_t RegionCountBeforeLastFullGC;
    size_t RegionCountAfterLastFullGC;
    size_t RegionCountBeforeFullGC;

    size_t RegionCountLastMonitor;
    TimePoint LastMonitor;
    TimePoint FullGCStart;

    double RegionOldFulledPerSecond;
    double RegionProcessedInFullGCPerSecond;
};

} // namespace gc
} // namespace hydra

#endif // _GC_SCHEDULER_H_