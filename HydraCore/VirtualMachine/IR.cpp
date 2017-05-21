#include "IR.h"

#include "IRInsts.h"

#include "GarbageCollection/GC.h"

#include <mutex>
#include <set>

namespace hydra
{
namespace vm
{

IRModule::IRModule(runtime::JSArray *stringsReferenced)
    : StringsReferenced(stringsReferenced)
{
    IRModuleGCHelper::GetInstance()->AddModule(this);
}

IRModule::~IRModule()
{
    IRModuleGCHelper::GetInstance()->RemoveModule(this);
}

size_t IRFunc::GetVarCount() const
{
    size_t varCount = 0;

    if (Blocks.empty())
    {
        return 0;
    }

    for (auto &inst : Blocks.front()->Insts)
    {
        if (inst->Is<ir::Alloca>())
        {
            ++varCount;
        }
        else
        {
            break;
        }
    }

    return varCount;
}

} // namespace vm
} // namespace hydra