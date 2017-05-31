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

void IRBlock::Dump(std::ostream &os)
{
    os << "\tblk_" << Index << ":" << std::endl;

    if (Dominator)
    {
        os << "\t\t[Dominator: blk_" << Dominator->Index << "]" << std::endl;
    }

    if (Precedences.size())
    {
        os << "\t\t[Precedences:";
        for (auto &prec : Precedences)
        {
            os << " blk_" << prec->Index;
        }
        os << "]" << std::endl;
    }

    for (auto &inst : Insts)
    {
        inst->Dump(os);
    }

    if (Condition)
    {
        os << "\t\tbranch $" << Condition->Index << ", blk_" << Consequent->Index << ", blk_" << Alternate->Index << std::endl;
    }
    else
    {
        os << "\t\tjump blk_" << Consequent->Index << std::endl;
    }
    os << std::endl;
}

void IRFunc::Dump(std::ostream &os)
{
    UpdateIndex();

    os << Name->ToString() << ":" << std::endl;
    for (auto &block : Blocks)
    {
        block->Dump(os);
    }
}

} // namespace vm
} // namespace hydra