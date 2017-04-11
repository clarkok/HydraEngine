#include "ThreadPool.h"

#include "AutoCounter.h"

namespace hydra
{

ThreadPool::ThreadPool(size_t threadCount)
    : LivingThreadCount(0), ActiveThreadCount(0), Threads(threadCount)
{
    std::generate(Threads.begin(), Threads.end(), [&]()
    {
        return std::thread(&ThreadPool::ThreadPoolWorker, this);
    });
}

ThreadPool::~ThreadPool()
{
    size_t livingThreadCount = LivingThreadCount.load();

    while (livingThreadCount--)
    {
        TaskQueue.Enqueue(new SelfDestroyTask());
    }

    for (auto &thread : Threads)
    {
        thread.join();
    }
}

void ThreadPool::ThreadPoolWorker()
{
    AutoCounter<size_t> autoLivingThreadCounter(LivingThreadCount);

    while (true)
    {
        TaskBase *task = nullptr;

        TaskQueue.Dequeue(task);

        if (dynamic_cast<SelfDestroyTask*>(task))
        {
            delete task;
            break;
        }

        AutoCounter<size_t> autoActiveThreadCounter(ActiveThreadCount);
        task->Execute();

        delete task;
    }
}

} // namespace hydra