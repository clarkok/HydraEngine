#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "Common/ThreadPool.h"

#include <chrono>

namespace hydra
{

using namespace std::chrono_literals;

TEST_CASE("ThreadPool", "[Common]")
{
    auto uut = ThreadPool::GetInstance();

    REQUIRE(uut->GetLivingThreadCount() == std::thread::hardware_concurrency());
    REQUIRE(uut->GetActiveThreadCount() == 0);

    SECTION("Dispatch instance task")
    {
        for (int i = 0; i < 10; ++i)
        {
            auto f = uut->Dispatch<int>([i]()
            {
                return i;
            });

            REQUIRE(f.get() == i);
        }
    }

    SECTION("Dispatch task with argument")
    {
        for (int i = 0; i < 10; ++i)
        {
            auto f = uut->Dispatch<int>([](int t) { return t; }, i);

            REQUIRE(f.get() == i);
        }
    }

    SECTION("Dispatch long term task")
    {
        std::vector<std::future<int>> futures;

        for (int i = 0; i < 10; ++i)
        {
            futures.push_back(uut->Dispatch<int>([i]()
            {
                std::this_thread::sleep_for(1s);
                return i;
            }));
        }

        REQUIRE(futures.size() == 10);

        for (int i = 0; i < 10; ++i)
        {
            REQUIRE(futures[i].get() == i);
        }
    }

    SECTION("Dispatch and wait")
    {
        std::vector<std::future<int>> futures;

        for (int i = 0; i < 10; ++i)
        {
            futures.push_back(uut->Dispatch<int>([i]()
            {
                return i;
            }));
        }

        REQUIRE(futures.size() == 10);

        std::this_thread::sleep_for(2s);

        for (int i = 0; i < 10; ++i)
        {
            REQUIRE(futures[i].get() == i);
        }
    }
}

}