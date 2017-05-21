#include "HydraCore/Core.hpp"

#include <iostream>

using namespace hydra;

int main(int argc, const char **argv)
{
    std::cout << "Hail Hydra!" << std::endl;

    if (argc < 2)
    {
        std::cerr << "Missing parameter" << std::endl;
    }

    auto VM = vm::VM::GetInstance();
    gc::ThreadAllocator allocator(gc::Heap::GetInstance());
    runtime::semantic::Initialize(allocator);

    VM->CompileToTask(allocator, argv[1]);
    VM->Execute(allocator);

    VM->Stop();

    return 0;
}
