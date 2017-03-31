#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "Common/HydraCore.h"
#include "GarbageCollection/Region.h"
#include "GarbageCollection/ThreadAllocator.h"

#include "TestHeapObject.h"

#include "Common/Platform.h"

using namespace hydra;

TEST_CASE("Region", "[GC]")
{
    size_t level = gc::ThreadAllocator::GetLevelFromSize(sizeof(TestHeapObject));
    gc::Region *uut = gc::Region::New(level);

    REQUIRE(uut != nullptr);

    SECTION("validate initialized cell property")
    {
        for (auto cell : *uut)
        {
            REQUIRE(cell->IsInUse() == false);
            REQUIRE(cell->IsLarge() == false);
            REQUIRE(cell->GetGCState() == gc::GCState::GC_WHITE);
        }
    }

    SECTION("allocate one object")
    {
        TestHeapObject *allocated = uut->Allocate<TestHeapObject>();

        REQUIRE(uut == gc::Region::GetRegionOfObject(allocated));

        REQUIRE(allocated->IsInUse() == true);
        REQUIRE(allocated->IsLarge() == false);
        REQUIRE(allocated->GetGCState() == gc::GCState::GC_WHITE);

        REQUIRE(reinterpret_cast<uintptr_t>(allocated) ==
            reinterpret_cast<uintptr_t>(*(uut->begin())));
    }

    SECTION("total allocated count")
    {
        size_t count = 0;

        while (uut->Allocate<TestHeapObject>())
        {
            ++count;
        }

        REQUIRE(count == gc::Region::CellCountFromLevel(level));

        for (auto cell : *uut)
        {
            REQUIRE(cell->IsInUse() == true);
            REQUIRE(cell->IsLarge() == false);
            REQUIRE(cell->GetGCState() == gc::GCState::GC_WHITE);
            REQUIRE(dynamic_cast<TestHeapObject*>(cell) != nullptr);
        }
    }

    SECTION("young sweep with no old object")
    {
        while (uut->Allocate<TestHeapObject>())
        { }

        for (auto cell : *uut)
        {
            REQUIRE(cell->IsInUse() == true);
            REQUIRE(cell->IsLarge() == false);
            REQUIRE(cell->GetGCState() == gc::GCState::GC_WHITE);
            REQUIRE(dynamic_cast<TestHeapObject*>(cell) != nullptr);
        }

        size_t livingObjectCount = uut->YoungSweep();
        REQUIRE(livingObjectCount == 0);

        for (auto cell : *uut)
        {
            REQUIRE(cell->IsInUse() == false);
            REQUIRE(cell->IsLarge() == false);
            REQUIRE(cell->GetGCState() == gc::GCState::GC_WHITE);
        }
    }

    SECTION("young gc with old objects")
    {
        while (uut->Allocate<TestHeapObject>())
        { }

        size_t oldObjectCount = 10;
        for (auto cell : *uut)
        {
            if (oldObjectCount-- == 0)
            {
                break;
            }

            cell->SetGCState(gc::GCState::GC_DARK);
            uut->IncreaseOldObjectCount();
        }

        size_t livingObjectCount = uut->YoungSweep();
        REQUIRE(livingObjectCount == 10);

        {
            auto iter = uut->begin();
            for (size_t i = 0; i < 10; ++i, ++iter)
            {
                auto cell = *iter;

                REQUIRE(cell->IsInUse() == true);
                REQUIRE(cell->IsLarge() == false);
                REQUIRE(cell->GetGCState() == gc::GCState::GC_DARK);
            }

            for (; iter != uut->end(); ++iter)
            {
                auto cell = *iter;

                REQUIRE(cell->IsInUse() == false);
                REQUIRE(cell->IsLarge() == false);
                REQUIRE(cell->GetGCState() == gc::GCState::GC_WHITE);
            }
        }

        size_t allocatedCount = 0;
        while (uut->Allocate<TestHeapObject>())
        {
            allocatedCount++;
        }

        REQUIRE(livingObjectCount + allocatedCount == gc::Region::CellCountFromLevel(level));

        {
            auto iter = uut->begin();
            for (size_t i = 0; i < 10; ++i, ++iter)
            {
                auto cell = *iter;

                REQUIRE(cell->IsInUse() == true);
                REQUIRE(cell->IsLarge() == false);
                REQUIRE(cell->GetGCState() == gc::GCState::GC_DARK);
            }

            for (; iter != uut->end(); ++iter)
            {
                auto cell = *iter;

                REQUIRE(cell->IsInUse() == true);
                REQUIRE(cell->IsLarge() == false);
                REQUIRE(cell->GetGCState() == gc::GCState::GC_WHITE);
            }
        }
    }

    SECTION("InRegion Test")
    {
        auto *obj = uut->Allocate<TestHeapObject>();

        gc::Cell *cell = nullptr;

        REQUIRE(gc::Region::IsInRegion(obj, cell));
        REQUIRE(cell == obj);

        REQUIRE(gc::Region::IsInRegion(uut, cell));
        REQUIRE(cell == nullptr);

        void *ptrOnStack = &obj;
        REQUIRE(!gc::Region::IsInRegion(ptrOnStack, cell));
    }

    gc::Region::Delete(uut);
}

TEST_CASE("ForeachWordOnStack", "[GC]")
{
    platform::ForeachWordOnStack([](void *ptr)
    {
        Logger::GetInstance()->Log() << ptr << "\t" << std::hex << *reinterpret_cast<uintptr_t*>(ptr);
    });
}

struct EventListener : Catch::TestEventListenerBase
{
    using TestEventListenerBase::TestEventListenerBase;

    virtual void testRunEnded(Catch::TestRunStats const &) override
    {
        Logger::GetInstance()->Shutdown();
    }
};

CATCH_REGISTER_LISTENER(EventListener);
