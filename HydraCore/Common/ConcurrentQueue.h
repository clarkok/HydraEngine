#ifndef _CONCURRENT_QUEUE_H_
#define _CONCURRENT_QUEUE_H_

#include "HydraCore.h"

namespace hydra
{

namespace concurrent
{

template<typename T, size_t N = 1024, size_t SPIN_COUNT = 32>
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

        Initialized.store(true, std::memory_order_release);
    }

    bool IsInitialized()
    {
        return Initialized.load(std::memory_order_acquire);
    }

    void WaitForInitialize()
    {
        while (!IsInitialized());
    }

    bool TryEnqueue(const T &value)
    {
        WaitForInitialize();

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
            else if (seq < 0)
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
        WaitForInitialize();

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

            {
                std::unique_lock<std::mutex> lck(WaitMutex);
                WriteCV.wait(lck);
            }
        }
    }

    void Dequeue(T &value)
    {
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

            {
                std::unique_lock<std::mutex> lck(WaitMutex);
                ReadCV.wait(lck);
            }
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

    std::array<Cell, N> Buffer;
    std::atomic<size_t> Head;
    std::atomic<size_t> Tail;
    std::mutex WaitMutex;
    std::condition_variable ReadCV;
    std::condition_variable WriteCV;
    std::atomic<bool> Initialized;
};

} // namespace concurrent

} // namespace hydra

#endif // _CONCURRENT_QUEUE_H_