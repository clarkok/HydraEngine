#ifndef _CONCURRENT_QUEUE_H_
#define _CONCURRENT_QUEUE_H_

#include "HydraCore.h"

namespace hydra
{

namespace concurrent
{

template<typename T, size_t N = 1024, size_t SPIN_COUNT = 8>
class Queue
{
public:
    Queue()
        : Head(0), Tail(0), WaitMutex(), ReadCV(), WriteCV()
    {
        for (size_t i = 0; i < N; ++i)
        {
            Buffer[i].Seq.store(i, std::memory_order_relaxed);
        }
    }

    bool TryEnqueue(const T &value)
    {
        Cell *cell;
        size_t tail = Tail.load(std::memory_order_acquire);
        while (true)
        {
            cell = &Buffer[tail % N];
            size_t seq = cell->Seq.load(std::memory_order_acquire);
            if (seq == tail)
            {
                if (Tail.compare_exchange_weak(tail, tail + 1, std::memory_order_relaxed))
                    break;
            }
            else if (seq < tail)
            {
                return false;
            }
            else
            {
                tail = Tail.load(std::memory_order_relaxed);
            }
        }
        cell->Value = value;
        cell->Seq.store(tail + 1, std::memory_order_release);
        ReadCV.notify_one();
        return true;
    }

    bool TryDequeue(T &value)
    {
        Cell *cell;
        size_t head = Head.load(std::memory_order_relaxed);
        while (true)
        {
            cell = &Buffer[head % N];
            size_t seq = cell->Seq.load(std::memory_order_acquire);
            if (seq == head + 1)
            {
                if (Head.compare_exchange_weak(head, head + 1, std::memory_order_relaxed))
                    break;
            }
            else if (seq < head + 1)
            {
                return false;
            }
            else
            {
                head = Head.load(std::memory_order_relaxed);
            }
        }
        value = cell->Value;
        cell->Seq.store(head + N, std::memory_order_release);
        WriteCV.notify_one();
        return true;
    }

    void Enqueue(const T &value)
    {
        RetryController retry(WaitMutex, WriteCV);
        while (true)
        {
            size_t spinCount = SPIN_COUNT;
            while (spinCount-- > 0)
            {
                if (TryEnqueue(value))
                {
                    return;
                }
            }
            retry.Wait();
        }
    }

    void Dequeue(T &value)
    {
        RetryController retry(WaitMutex, ReadCV);
        while (true)
        {
            size_t spinCount = SPIN_COUNT;
            while (spinCount-- > 0)
            {
                if (TryDequeue(value))
                {
                    return;
                }
            }
            retry.Wait();
        }
    }

    size_t Count()
    {
        return Tail.load(std::memory_order_relaxed) - Head.load(std::memory_order_relaxed);
    }

    size_t Capacity() const
    {
        return N;
    }

private:
    struct Cell
    {
        std::atomic<size_t> Seq;
        T Value;
    };

    struct RetryController
    {
        RetryController(std::mutex &mutex, std::condition_variable &cv)
            : RetryCounter(0), Mutex(mutex), CV(cv)
        { }

        inline void Wait()
        {
            auto retry = RetryCounter++;
            if (retry < 5)
            {
                std::this_thread::yield();
            }
            else if (retry < 10)
            {
                std::unique_lock<std::mutex> lck(Mutex);
                CV.wait_for(lck, 3ms);
            }
            else
            {
                std::unique_lock<std::mutex> lck(Mutex);
                CV.wait_for(lck, 10ms);
            }
        }

        size_t RetryCounter;
        std::mutex &Mutex;
        std::condition_variable &CV;
    };

    std::array<Cell, N> Buffer;
    std::atomic<size_t> Head;
    std::atomic<size_t> Tail;
    std::mutex WaitMutex;
    std::condition_variable ReadCV;
    std::condition_variable WriteCV;
};

} // namespace concurrent

} // namespace hydra

#endif // _CONCURRENT_QUEUE_H_
