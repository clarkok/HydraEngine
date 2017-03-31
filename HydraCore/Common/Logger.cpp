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
    auto StartTime = std::chrono::high_resolution_clock::now();

    auto writeTime = [&](decltype(StartTime) time)
    {
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(time - StartTime).count();
        auto hour = us / 1000 / 1000 / 60 / 60;
        auto min = us / 1000 / 1000 / 60 % 60;
        auto sec = us / 1000 / 1000 % 60;
        auto ms = us / 1000 % 1000;
        us = us % 1000;

        fout << (hour < 10 ? "0" : "") << hour << ":"
            << (min < 10 ? "0" : "") << min << ":"
            << (sec < 10 ? "0" : "") << sec << "."
            << (ms < 10 ? "00" : ms < 100 ? "0" : "") << ms << "."
            << (us < 10 ? "00" : us < 100 ? "0" : "") << us;
    };

    while (!ShouldExit.load())
    {
        Entry entry;
        Queue.Dequeue(entry);

        writeTime(entry.TimePoint);
        fout << "\t" << entry.ThreadId << "\t" << entry.Message << std::endl;;
    }

    Entry entry;
    while (Queue.TryDequeue(entry))
    {
        writeTime(entry.TimePoint);
        fout << "\t" << entry.ThreadId << "\t" << entry.Message << std::endl;;
    }

    std::cout << "Logger shutdown" << std::endl;
}

} // namespace hydra