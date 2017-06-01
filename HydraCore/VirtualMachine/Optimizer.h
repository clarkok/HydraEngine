#ifndef _OPTIMIZER_H_
#define _OPTIMIZER_H_

#include "Common/HydraCore.h"
#include "IR.h"
#include "IRInsts.h"

namespace hydra
{
namespace vm
{

class Optimizer
{
public:
    static void RemoveAfterReturn(IRFunc *func);
    static void ControlFlowAnalyze(IRFunc *func);
    static void InlineScope(IRFunc *func);
    static void MemToReg(IRFunc *func);
    static void CleanupBlocks(IRFunc *func);

private:
    static void RemoveUnreferencedBlocks(IRFunc *func);
};

} // namespace vm
} // namespace hydra

#endif // _OPTIMIZER_H_