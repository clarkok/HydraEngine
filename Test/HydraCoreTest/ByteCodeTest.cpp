#define CATCH_CONFIG_MAIN
#include "Catch/include/catch.hpp"

#include "VirtualMachine/ByteCode.h"

namespace hydra
{

TEST_CASE("ByteCodeTest", "[vm]")
{
    std::string filename = "C:/Users/t-jingcz/Documents/HydraEngine/HydraCompiler/test/clouse.ir";
    gc::ThreadAllocator allocator(gc::Heap::GetInstance());

    vm::ByteCode uut(filename);

    auto irModule = uut.Load(allocator);

    REQUIRE(irModule->Functions.size() == 2);
}

}