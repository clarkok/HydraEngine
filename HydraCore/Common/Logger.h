#ifndef _LOGGER_H_
#define _LOGGER_H_

#include "Singleton.h"
#include "ConcurrentQueue.h"
#include "Common/HydraCore.h"

#include <thread>
#include <atomic>
#include <string>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iomanip>

namespace hydra
{

class Logger : public Singleton<Logger>
{
public:
#ifdef HYDRA_ENABLE_LOG
    class LoggerProxy : public std::stringstream
#else
    class LoggerProxy
#endif
    {
    public:
        ~LoggerProxy()
        {
#ifdef HYDRA_ENABLE_LOG
            if (Owner)
            {
                Owner->InsertEntry(TimePoint, str(), std::this_thread::get_id());
            }
#endif
        }

#ifndef HYDRA_ENABLE_LOG
        template <typename T>
        LoggerProxy &operator << (const T &)
        {
            return *this;
        }
#endif

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
                WriteLogAndUpdateLast("END", std::chrono::high_resolution_clock::now());
            }
        }

        inline void Phase(std::string phaseName)
        {
            hydra_assert(Logger,
                "Valid PerfSession should refer to a valid Logger");

            WriteLogAndUpdateLast(phaseName, std::chrono::high_resolution_clock::now());
        }

    private:
        PerfSession(Logger *logger, std::string name)
            : Logger(logger), Name(name), Start(std::chrono::high_resolution_clock::now()), Last(Start)
        {
            WriteLogAndUpdateLast("START", Start);
        }

        inline void WriteLogAndUpdateLast(std::string phaseName, std::chrono::time_point<std::chrono::high_resolution_clock> now)
        {
            double fromStart = std::chrono::duration_cast<std::chrono::microseconds>(now - Start).count() / 1000.0f;
            double fromLast = std::chrono::duration_cast<std::chrono::microseconds>(now - Last).count() / 1000.0f;
            Logger->Log() << "Perf " << std::fixed << std::setprecision(3) << fromStart <<
                "(+" << fromLast << ") " << Name << "::" << phaseName;

            Last = now;
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