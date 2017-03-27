#include "Logger.h"
#include <iostream>
#include <fstream>

namespace hydra
{

Logger::Logger()
    : ShouldExit(false),
    Queue(),
    WriteThread(&Logger::Write, this)
{ }

Logger::~Logger()
{
    ShouldExit.store(true);
    WriteThread.join();
}

void Logger::Write()
{
    while (!ShouldExit.load())
    {
        Entry entry;
        Queue.Dequeue(entry);

        std::cout << entry.TimePoint.time_since_epoch().count() << "\t" << entry.ThreadId << "\t" << entry.Message << std::endl;;
    }
}

} // namespace hydra