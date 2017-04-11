#ifndef _AUTO_COUNTER_H_
#define _AUTO_COUNTER_H_

#include "HydraCore.h"

#include <atomic>
#include <type_traits>

namespace hydra
{

template <typename T>
class AutoCounter
{
    static_assert(std::is_integral<T>::value,
        "T should be integral");

public:
    AutoCounter(std::atomic<T> &counter)
        : Counter(counter)
    {
        Counter.fetch_add(1);
    }

    ~AutoCounter()
    {
        Counter.fetch_add(-1);
    }

private:
    std::atomic<T> &Counter;
};

}

#endif // _AUTO_COUNTER_H_