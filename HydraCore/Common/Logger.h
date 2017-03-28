#ifndef _LOGGER_H_
#define _LOGGER_H_

#include "Singleton.h"
#include "ConcurrentQueue.h"

#include <thread>
#include <atomic>
#include <string>
#include <sstream>
#include <algorithm>
#include <chrono>

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

    class PerfSession
    {
    public:
        PerfSession(PerfSession &&other)
            : Logger(other.Logger), Name(other.Name), Start(other.Start), Last(other.Last)
        {
            other.Logger = nullptr;
        }

        ~PerfSession()
        {
            if (Logger)
            {
                auto Now = std::chrono::high_resolution_clock::now();
                Logger->Log() << "PerfSession " << Name << "::END\t"
                    << std::chrono::duration_cast<std::chrono::microseconds>(Now - Start).count() / 1000.0f << "ms "
                    << "(+" << std::chrono::duration_cast<std::chrono::microseconds>(Now - Last).count() / 1000.0f << "ms)";
            }
        }

        inline void Phase(std::string phaseName)
        {
            hydra_assert(Logger,
                "Valid PerfSession should refer to a valid Logger");

            auto Now = std::chrono::high_resolution_clock::now();
            Logger->Log() << "PerfSession " << Name << "::" << phaseName << "\t"
                << std::chrono::duration_cast<std::chrono::microseconds>(Now - Start).count() / 1000.0f << "ms "
                << "(+" << std::chrono::duration_cast<std::chrono::microseconds>(Now - Last).count() / 1000.0f << "ms)";
            Last = Now;
        }

    private:
        PerfSession(Logger *logger, std::string name)
            : Logger(logger), Name(name), Start(std::chrono::high_resolution_clock::now()), Last(Start)
        {
            Logger->Log() << "PerfSession " << Name << "::START";
        }

        Logger *Logger;
        std::string Name;
        std::chrono::time_point<std::chrono::high_resolution_clock> Start;
        std::chrono::time_point<std::chrono::high_resolution_clock> Last;

        friend class Logger;
    };

    Logger();
    ~Logger();

    inline LoggerProxy Log()
    {
        LoggerProxy ret(this);
        return std::move(ret);
    }

    inline PerfSession Perf(std::string name)
    {
        PerfSession ret(this, name);
        return std::move(ret);
    }

    void Shutdown();

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