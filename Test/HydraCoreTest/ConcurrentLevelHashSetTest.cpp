#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "Common/ConcurrentLevelHashSet.h"

namespace hydra
{

size_t hash(int *ptr)
{
    return reinterpret_cast<uintptr_t>(ptr);
}

TEST_CASE("ConcurrentLevelHashSet", "Common")
{
    int i;
    concurrent::LevelHashSet<int> uut;

    bool hasValue = uut.Add(&i);

    REQUIRE(!hasValue);

    hasValue = uut.Has(&i);

    REQUIRE(hasValue);

    hasValue = uut.Add(&i);

    REQUIRE(hasValue);

    hasValue = uut.Remove(&i);

    REQUIRE(hasValue);

    hasValue = uut.Has(&i);

    REQUIRE(!hasValue);

    SECTION("Large amount in loop")
    {
        uintptr_t NUMBER = 10000;

        for (uintptr_t i = sizeof(int); i < NUMBER; i += sizeof(int))
        {
            CAPTURE(i);

            bool hasValue = uut.Add(reinterpret_cast<int*>(i));
            REQUIRE(!hasValue);

            hasValue = uut.Has(reinterpret_cast<int*>(i));
            REQUIRE(hasValue);

            hasValue = uut.Add(reinterpret_cast<int*>(i));
            REQUIRE(hasValue);
        }
    }

    SECTION("Large amount out loop")
    {
        uintptr_t NUMBER = 10000;

        for (uintptr_t i = sizeof(int); i < NUMBER; i += sizeof(int))
        {
            bool hasValue = uut.Add(reinterpret_cast<int*>(i));
            REQUIRE(!hasValue);
        }

        for (uintptr_t i = sizeof(int); i < NUMBER; i += sizeof(int))
        {
            hasValue = uut.Has(reinterpret_cast<int*>(i));
            REQUIRE(hasValue);
        }

        for (uintptr_t i = sizeof(int); i < NUMBER; i += sizeof(int))
        {
            hasValue = uut.Add(reinterpret_cast<int*>(i));
            REQUIRE(hasValue);
        }
    }

    SECTION("Mult-thread 1-1")
    {
        std::atomic<bool> shouldExit{ false };

        constexpr uintptr_t NUMBER = 100000;

        std::thread checkThread([&]()
        {
            uintptr_t checked = sizeof(int);
            while (checked < NUMBER)
            {
                while (!uut.Has(reinterpret_cast<int*>(checked))) { }
                checked += sizeof(int);
            }
        });

        std::thread addThread([&]()
        {
            uintptr_t added = sizeof(int);
            while (added < NUMBER)
            {
                bool hasValue = uut.Add(reinterpret_cast<int*>(added));
                REQUIRE(!hasValue);
                added += sizeof(int);
            }
        });

        addThread.join();
        checkThread.join();

        for (uintptr_t i = sizeof(int); i < NUMBER; i += sizeof(int))
        {
            REQUIRE(uut.Has(reinterpret_cast<int*>(i)));
        }
    }

    SECTION("Mult-thread 2-1")
    {
        std::atomic<bool> shouldExit{ false };

        constexpr uintptr_t NUMBER = 100000;
        constexpr uintptr_t NUMBER_MID = (NUMBER / 2) & -sizeof(int);

        std::thread checkThread([&]()
        {
            uintptr_t checked = sizeof(int);
            while (checked < NUMBER)
            {
                while (!uut.Has(reinterpret_cast<int*>(checked))) { }
                checked += sizeof(int);
            }
        });

        std::thread addThread1([&]()
        {
            uintptr_t added = sizeof(int);
            while (added < NUMBER_MID)
            {
                bool hasValue = uut.Add(reinterpret_cast<int*>(added));
                added += sizeof(int);
            }
        });

        std::thread addThread2([&]()
        {
            uintptr_t added = NUMBER_MID;
            while (added < NUMBER)
            {
                bool hasValue = uut.Add(reinterpret_cast<int*>(added));
                added += sizeof(int);
            }
        });

        addThread1.join();
        addThread2.join();
        checkThread.join();

        for (uintptr_t i = sizeof(int); i < NUMBER; i += sizeof(int))
        {
            REQUIRE(uut.Has(reinterpret_cast<int*>(i)));
        }
    }
}

}