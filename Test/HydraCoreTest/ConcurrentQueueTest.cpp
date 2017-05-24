#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "Common/ConcurrentQueue.h"

#include <vector>
#include <mutex>
#include <algorithm>

namespace hydra
{

TEST_CASE("ConcurrentQueue", "Common")
{
    static constexpr int TEST_COUNT = 500000;

    concurrent::Queue<int, 16> uut;

    REQUIRE(uut.Count() == 0);

    SECTION("Basic")
    {
        int expected = 0;
        int value;

        for (int i = 0; i < TEST_COUNT; ++i)
        {
            if (!uut.TryEnqueue(i))
            {
                while (uut.TryDequeue(value))
                {
                    REQUIRE(value == expected++);
                }
                uut.TryEnqueue(i);
            }
        }

        while (uut.TryDequeue(value))
        {
            REQUIRE(value == expected++);
        }
    }

    SECTION("Multiple Threads")
    {
        std::vector<int> generated(TEST_COUNT);
        int counter = 0;
        std::generate(generated.begin(), generated.end(), [&]()
        {
            return counter++;
        });

        std::mutex collectedMutex;
        std::vector<int> collected;
        collected.reserve(TEST_COUNT);

        auto consumer = [&]()
        {
            int count = TEST_COUNT / 2;
            std::vector<int> local;
            local.reserve(count);

            while (count--)
            {
                int value;
                uut.Dequeue(value);
                local.push_back(value);
            }

            {
                std::unique_lock<std::mutex> lck(collectedMutex);
                std::copy(local.begin(), local.end(), std::back_inserter(collected));
            }
        };

        auto producer = [&](int start)
        {
            int count = TEST_COUNT / 2;
            while (count--)
            {
                uut.Enqueue(generated[start++]);
            }
        };

        std::thread c1(consumer);
        std::thread c2(consumer);
        std::thread p1(producer, 0);
        std::thread p2(producer, TEST_COUNT / 2);

        p1.join();
        p2.join();
        c1.join();
        c2.join();

        std::sort(collected.begin(), collected.end());

        for (int i = 0; i < TEST_COUNT; ++i)
        {
            REQUIRE(i == collected[i]);
        }
    }

    SECTION("Multiple Threads Slow Producer")
    {
        std::vector<int> generated(TEST_COUNT);
        int counter = 0;
        std::generate(generated.begin(), generated.end(), [&]()
        {
            return counter++;
        });

        std::mutex collectedMutex;
        std::vector<int> collected;
        collected.reserve(TEST_COUNT);

        auto consumer = [&]()
        {
            int count = TEST_COUNT / 2;
            std::vector<int> local;
            local.reserve(count);

            while (count--)
            {
                int value;
                uut.Dequeue(value);
                local.push_back(value);
            }

            {
                std::unique_lock<std::mutex> lck(collectedMutex);
                std::copy(local.begin(), local.end(), std::back_inserter(collected));
            }
        };

        auto producer = [&](int start)
        {
            int count = TEST_COUNT / 2;
            while (count--)
            {
                uut.Enqueue(generated[start++]);
                std::this_thread::yield();
            }
        };

        std::thread c1(consumer);
        std::thread c2(consumer);
        std::thread p1(producer, 0);
        std::thread p2(producer, TEST_COUNT / 2);

        p1.join();
        p2.join();
        c1.join();
        c2.join();

        std::sort(collected.begin(), collected.end());

        for (int i = 0; i < TEST_COUNT; ++i)
        {
            REQUIRE(i == collected[i]);
        }
    }

    SECTION("Multiple Threads Slow Consumer")
    {
        std::vector<int> generated(TEST_COUNT);
        int counter = 0;
        std::generate(generated.begin(), generated.end(), [&]()
        {
            return counter++;
        });

        std::mutex collectedMutex;
        std::vector<int> collected;
        collected.reserve(TEST_COUNT);

        auto consumer = [&]()
        {
            int count = TEST_COUNT / 2;
            std::vector<int> local;
            local.reserve(count);

            while (count--)
            {
                int value;
                uut.Dequeue(value);
                local.push_back(value);
                std::this_thread::yield();
            }

            {
                std::unique_lock<std::mutex> lck(collectedMutex);
                std::copy(local.begin(), local.end(), std::back_inserter(collected));
            }
        };

        auto producer = [&](int start)
        {
            int count = TEST_COUNT / 2;
            while (count--)
            {
                uut.Enqueue(generated[start++]);
            }
        };

        std::thread c1(consumer);
        std::thread c2(consumer);
        std::thread p1(producer, 0);
        std::thread p2(producer, TEST_COUNT / 2);

        p1.join();
        p2.join();
        c1.join();
        c2.join();

        std::sort(collected.begin(), collected.end());

        for (int i = 0; i < TEST_COUNT; ++i)
        {
            REQUIRE(i == collected[i]);
        }
    }
}

}