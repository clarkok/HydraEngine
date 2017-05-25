#include "HydraCore/Core.hpp"

#include <iostream>

using namespace hydra;

int main(int argc, const char **argv)
{
    // ensure initialization order
    {
        Logger::GetInstance();
        gc::Heap::GetInstance();
        vm::IRModuleGCHelper::GetInstance();
        runtime::semantic::Initialize();
        vm::VM::GetInstance();
    }

    std::cout << "Hail Hydra!" << std::endl;

    if (argc < 2)
    {
        std::cerr << "Missing parameter" << std::endl;
        return 1;
    }

    auto VM = vm::VM::GetInstance();
    gc::ThreadAllocator allocator(gc::Heap::GetInstance());

    VM->CompileToTask(allocator, argv[1]);
    VM->Execute(allocator);

    allocator.SetInactive([](){});
    VM->Stop();

    return 0;
}
