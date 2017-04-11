#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include "HydraCore.h"

#include "ConcurrentQueue.h"
#include "Singleton.h"

#include <functional>
#include <thread>
#include <vector>
#include <future>
#include <atomic>

namespace hydra
{

class ThreadPool : public Singleton<ThreadPool>
{
public:
    ThreadPool(size_t threadCount = std::thread::hardware_concurrency());
    ~ThreadPool();

    inline size_t GetLivingThreadCount()
    {
        return LivingThreadCount.load();
    }

    inline size_t GetActiveThreadCount()
    {
        return ActiveThreadCount.load();
    }

    template <typename T_Ret, typename T_Func, typename ...T_Args>
    std::future<T_Ret> Dispatch(T_Func func, T_Args ...args)
    {
        auto task = new Task<T_Func, T_Ret, T_Args...>(func, args...);
        auto ret = task->GetFuture();
        TaskQueue.Enqueue(task);

        return ret;
    }

private:
    struct TaskBase
    {
        virtual ~TaskBase() = default;
        virtual void Execute() = 0;
    };

    template <typename T_Func, typename T_Ret, typename ...T_Args>
    struct Task : public TaskBase
    {
        Task(T_Func func, T_Args ...args)
            : Wrapper(std::bind(func, args...))
        { }

        virtual ~Task() = default;

        inline std::future<T_Ret> GetFuture()
        {
            return Wrapper.get_future();
        }

        virtual void Execute() override final
        {
            Wrapper();
        }

        std::packaged_task<T_Ret()> Wrapper;
    };

    struct SelfDestroyTask : public TaskBase
    {
        virtual void Execute() override final
        {
            trap("SelfDestroyTask should not be executed");
        }
    };

    void ThreadPoolWorker();

    std::atomic<size_t> LivingThreadCount;
    std::atomic<size_t> ActiveThreadCount;
    std::vector<std::thread> Threads;
    concurrent::Queue<TaskBase *> TaskQueue;
};

} // namespace hydra

#endif // _THREAD_POOL_H_