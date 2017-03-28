#include "Logger.h"
#include <iostream>
#include <fstream>

std::ofstream fout("./log.txt");

namespace hydra
{

Logger::Logger()
    : ShouldExit(false),
    Queue(),
    WriteThread(&Logger::Write, this)
{ }

Logger::~Logger()
{
    if (!ShouldExit.exchange(true))
    {
        WriteThread.join();
    }
}

void Logger::Shutdown()
{
    if (!ShouldExit.exchange(true))
    {
        Log() << "Logger shutdown requested";
        WriteThread.join();
    }
}

void Logger::Write()
{
    while (!ShouldExit.load())
    {
        Entry entry;
        Queue.Dequeue(entry);

        /* std::cout */ fout << entry.TimePoint.time_since_epoch().count() << "\t" << entry.ThreadId << "\t" << entry.Message << std::endl;;
    }

    Entry entry;
    while (Queue.TryDequeue(entry))
    {
        std::cout << entry.TimePoint.time_since_epoch().count() << "\t" << entry.ThreadId << "\t" << entry.Message << std::endl;;
    }

    std::cout << "Logger shutdown" << std::endl;
}

} // namespace hydra