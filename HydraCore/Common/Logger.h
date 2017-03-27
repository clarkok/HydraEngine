#ifndef _LOGGER_H_
#define _LOGGER_H_

#include "Singleton.h"
#include "ConcurrentQueue.h"

#include <thread>
#include <atomic>
#include <string>
#include <sstream>
#include <algorithm>

namespace hydra
{

class Logger : public Singleton<Logger>
{
public:
    class LoggerProxy : public std::stringstream
    {
    public:
        ~LoggerProxy()
        {
            if (Owner)
            {
                Owner->InsertEntry(TimePoint, str(), std::this_thread::get_id());
            }
        }

    private:
        explicit LoggerProxy(Logger *owner)
            : Owner(owner), TimePoint(std::chrono::high_resolution_clock::now())
        { }

        LoggerProxy(const LoggerProxy &) = delete;
        LoggerProxy(LoggerProxy &&other)
            : Owner(other.Owner), TimePoint(other.TimePoint)
        {
            other.Owner = nullptr;
        }
        LoggerProxy & operator =(const LoggerProxy &) = delete;
        LoggerProxy & operator =(LoggerProxy &&) = default;

        Logger *Owner;
        std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;

        friend class Logger;
    };

    Logger();
    ~Logger();

    LoggerProxy Log()
    {
        LoggerProxy ret(this);
        return std::move(ret);
    }

private:
    struct Entry
    {
        std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;
        std::string Message;
        std::thread::id ThreadId;
    };

    inline void InsertEntry(
        std::chrono::time_point<std::chrono::high_resolution_clock>timePoint,
        std::string message,
        std::thread::id threadId)
    {
        Queue.Enqueue({ timePoint, message, threadId });
    }

    std::atomic<bool> ShouldExit;
    concurrent::Queue<Entry> Queue;
    std::thread WriteThread;
    void Write();
};

}

#endif // _LOGGER_H_