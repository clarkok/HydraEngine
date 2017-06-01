#include "HydraCore/Core.hpp"

#include "HydraCore/VirtualMachine/ByteCode.h"
#include "HydraCore/VirtualMachine/Optimizer.h"

#include <iostream>
#include <fstream>

using namespace hydra;

int main(int argc, const char **argv)
{
    {
        Logger::GetInstance();
        gc::Heap::GetInstance();
        vm::IRModuleGCHelper::GetInstance();
        runtime::semantic::Initialize();
    }

    if (argc < 2)
    {
        std::cerr << "no input file" << std::endl;
        return 1;
    }

    gc::ThreadAllocator allocator(gc::Heap::GetInstance());

    vm::ByteCode byteCode(argv[1]);
    auto irModule = byteCode.Load(allocator);

    // remove after return
    {
        for (auto &func : irModule->Functions)
        {
            vm::Optimizer::RemoveAfterReturn(func.get());
        }
        std::ofstream fout(std::string(argv[1]) + ".remove-after-return.tir");
        for (auto &func : irModule->Functions)
        {
            func->Dump(fout);
        }
    }

    // control flow analyze
    {
        for (auto &func : irModule->Functions)
        {
            vm::Optimizer::ControlFlowAnalyze(func.get());
        }
        std::ofstream fout(std::string(argv[1]) + ".control-flow-analyze.tir");
        for (auto &func : irModule->Functions)
        {
            func->Dump(fout);
        }
    }

    // inline scope
    {
        for (auto &func : irModule->Functions)
        {
            vm::Optimizer::InlineScope(func.get());
        }
        std::ofstream fout(std::string(argv[1]) + ".inline-scope.tir");
        for (auto &func : irModule->Functions)
        {
            func->Dump(fout);
        }
    }

    // mem to reg
    {
        for (auto &func : irModule->Functions)
        {
            vm::Optimizer::MemToReg(func.get());
        }
        std::ofstream fout(std::string(argv[1]) + ".mem-to-reg.tir");
        for (auto &func : irModule->Functions)
        {
            func->Dump(fout);
        }
    }

    // cleanup blocks
    {
        for (auto &func : irModule->Functions)
        {
            vm::Optimizer::CleanupBlocks(func.get());
        }
        std::ofstream fout(std::string(argv[1]) + ".cleanup-blocks.tir");
        for (auto &func : irModule->Functions)
        {
            func->Dump(fout);
        }
    }

    Logger::GetInstance()->Shutdown();
    gc::Heap::GetInstance()->Shutdown();

    return 0;
}