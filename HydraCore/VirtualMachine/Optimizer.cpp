#include "Optimizer.h"

#include <stack>
#include <queue>
#include <map>

#ifdef NULL
#undef NULL
#endif

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#ifdef IN
#undef IN
#endif

#ifdef THIS
#undef THIS
#endif

namespace hydra
{
namespace vm
{

void Optimizer::InitialOptimize(IRFunc *func)
{
    RemoveAfterReturn(func);
    ControlFlowAnalyze(func);
    InlineScope(func);
    ArgToLocalAllocaAndMoveCapture(func);
    MemToReg(func);
    CleanupBlocks(func);
    LoopAnalyze(func);
    RemoveMove(func);
    RemoveLoopInvariant(func);
}

void Optimizer::RemoveAfterReturn(IRFunc *func)
{
    for (auto &block : func->Blocks)
    {
        auto returnIter = std::find_if(
            block->Insts.begin(),
            block->Insts.end(),
            [](const std::unique_ptr<IRInst> &inst)
            {
                return inst->Is<ir::Return>();
            });

        if (returnIter != block->Insts.end())
        {
            ++returnIter;
            block->Insts.erase(returnIter, block->Insts.end());
            block->Condition.Reset();
            block->Consequent.Reset();
            block->Alternate.Reset();
        }
    }

    RemoveUnreferencedBlocks(func);
}

void Optimizer::ControlFlowAnalyze(IRFunc *func)
{
#pragma region Analyze precedences and dominator 
    auto findNewDominator = [&](IRBlock *a, IRBlock *b) -> IRBlock *
    {
        std::set<IRBlock *> path;
        while (a)
        {
            path.insert(a);
            a = a->Dominator.Get();
        }

        while (b)
        {
            if (path.find(b) != path.end())
            {
                return b;
            }
            b = b->Dominator.Get();
        }

        hydra_trap("dominator not found");
    };

    {
        size_t index = 0;
        for (auto &block : func->Blocks)
        {
            block->Index = index++;
        }
    }

    std::function<void(IRBlock *)> dfs = [&](IRBlock *block)
    {
        if (block->Consequent)
        {
            block->Consequent->Precedences.push_back(block);
            if (block->Consequent->Dominator)
            {
                block->Consequent->Dominator =
                    findNewDominator(block->Consequent->Dominator.Get(), block);
            }
            else
            {
                block->Consequent->Dominator = block;
                dfs(block->Consequent.Get());
            }
        }

        if (block->Alternate)
        {
            block->Alternate->Precedences.push_back(block);
            if (block->Alternate->Dominator)
            {
                block->Alternate->Dominator =
                    findNewDominator(block->Alternate->Dominator.Get(), block);
            }
            else
            {
                block->Alternate->Dominator = block;
                dfs(block->Alternate.Get());
            }
        }
    };

    dfs(func->Blocks.front().get());

    for (auto &block : func->Blocks)
    {
        block->Precedences.unique([](const IRBlock::Ref &a, const IRBlock::Ref &b)
        {
            return a.Get() == b.Get();
        });
    }
#pragma endregion

#pragma region Analyze scope and endScope
    {
        std::vector<bool> visited(func->Blocks.size(), false);
        std::queue<IRBlock *> blockQueue;
        blockQueue.push(func->Blocks.front().get());

        while (!blockQueue.empty())
        {
            auto block = blockQueue.front(); blockQueue.pop();
            if (visited[block->Index])
            {
                continue;
            }
            visited[block->Index] = true;

            if (block->Dominator)
            {
                block->Scope = block->Dominator->EndScope;
            }

            ir::PushScope* scope = block->Scope->As<ir::PushScope>();
            for (auto &inst : block->Insts)
            {
                if (inst->Is<ir::PushScope>())
                {
                    if (scope)
                    {
                        inst->As<ir::PushScope>()->UpperScope = scope;
                        scope->InnerScopes.insert(inst->As<ir::PushScope>());
                    }

                    scope = inst->As<ir::PushScope>();
                    scope->OwnerBlock = block;
                }
                else if (inst->Is<ir::PopScope>())
                {
                    inst->As<ir::PopScope>()->Scope = scope;
                    scope = scope->UpperScope->As<ir::PushScope>();
                }
                else if (inst->Is<ir::Capture>())
                {
                    inst->As<ir::Capture>()->Scope = scope;
                }
            }
            block->EndScope = scope;

            if (block->Consequent)
            {
                blockQueue.push(block->Consequent.Get());
            }
            if (block->Alternate)
            {
                blockQueue.push(block->Alternate.Get());
            }
        }
    }

    // check
    for (auto &block : func->Blocks)
    {
        for (auto &precedence : block->Precedences)
        {
            hydra_assert(precedence->EndScope == block->Scope,
                "Scope should match");
        }
    }
#pragma endregion
}

void Optimizer::InlineScope(IRFunc *func)
{
    std::map<ir::PushScope *, std::list<std::unique_ptr<IRInst>>::iterator> scopeToIterator;
    std::set<ir::PushScope *> unableToInline;
    std::list<std::unique_ptr<IRInst>>::iterator entryIterator = std::find_if(
        func->Blocks.front()->Insts.begin(),
        func->Blocks.front()->Insts.end(),
        [&](const std::unique_ptr<IRInst> &inst) { return !inst->Is<ir::Alloca>(); }
    );

    // filter out inlinable scopes
    for (auto &block : func->Blocks)
    {
        ir::PushScope *scope = block->Scope->As<ir::PushScope>();

        // for (auto &inst : block->Insts)
        for (auto iter = block->Insts.begin(); iter != block->Insts.end(); ++iter)
        {
            std::unique_ptr<IRInst> &inst = *iter;

            if (inst->Is<ir::PushScope>())
            {
                scope = inst->As<ir::PushScope>();
                scopeToIterator.insert(
                    std::make_pair(scope, iter));
            }
            else if (inst->Is<ir::PopScope>())
            {
                scope = scope->UpperScope->As<ir::PushScope>();
            }
            else if (inst->Is<ir::Func>())
            {
                auto func = inst->As<ir::Func>();
                for (auto &capture : func->Captured)
                {
                    auto currentScope = scope;
                    auto inst = capture.Get();
                    while (inst->Is<ir::Capture>())
                    {
                        auto captureInst = inst->As<ir::Capture>();
                        currentScope = captureInst->Scope->As<ir::PushScope>();

                        if (!currentScope)
                        {
                            break;
                        }

                        auto captureIter = currentScope->Captured.begin();
                        std::advance(captureIter, captureInst->Index);
                        inst = captureIter->Get();
                    }

                    if (currentScope)
                    {
                        unableToInline.insert(currentScope);
                    }
                }
            }
            else if (inst->Is<ir::Arrow>())
            {
                auto func = inst->As<ir::Arrow>();
                for (auto &capture : func->Captured)
                {
                    auto currentScope = scope;
                    auto inst = capture.Get();
                    while (inst->Is<ir::Capture>())
                    {
                        auto captureInst = inst->As<ir::Capture>();
                        currentScope = captureInst->Scope->As<ir::PushScope>();

                        if (!currentScope)
                        {
                            break;
                        }

                        auto captureIter = currentScope->Captured.begin();
                        std::advance(captureIter, captureInst->Index);
                        inst = captureIter->Get();
                    }

                    if (currentScope)
                    {
                        unableToInline.insert(currentScope);
                    }
                }
            }
        }
    }

    // move alloca
    for (auto &block : func->Blocks)
    {
        ir::PushScope *scope = block->Scope->As<ir::PushScope>();
        bool inlinable = !scope ? false : (unableToInline.find(scope) == unableToInline.end());

        for (auto iter = block->Insts.begin(); iter != block->Insts.end();)
        {
            std::unique_ptr<IRInst> &inst = *iter;

            if (inst->Is<ir::PushScope>())
            {
                scope = inst->As<ir::PushScope>();
                inlinable = (unableToInline.find(scope) == unableToInline.end());
                ++iter;
            }
            else if (inst->Is<ir::PopScope>())
            {
                scope = scope->UpperScope->As<ir::PushScope>();
                inlinable = !scope ? false : (unableToInline.find(scope) == unableToInline.end());
                ++iter;
            }
            else if (inlinable && inst->Is<ir::Alloca>())
            {
                ir::Alloca *removed = dynamic_cast<ir::Alloca*>(inst.release());
                hydra_assert(removed, "removed should not be nullptr");

                iter = block->Insts.erase(iter);

                auto upperScope = scope;
                while (upperScope && (unableToInline.find(upperScope) == unableToInline.end()))
                {
                    upperScope = upperScope->UpperScope->As<ir::PushScope>();
                }
                if (upperScope)
                {
                    upperScope->Size++;
                }

                if (upperScope)
                {
                    scopeToIterator[upperScope] = upperScope->OwnerBlock->Insts.emplace(
                        scopeToIterator[upperScope],
                        removed
                    );
                }
                else
                {
                    entryIterator = func->Blocks.front()->Insts.emplace(
                        entryIterator,
                        removed
                    );
                }
            }
            else if (scope && inst->Is<ir::Capture>())
            {
                ir::Capture *capture = inst->As<ir::Capture>();
                auto replaceIter = scope->Captured.begin();
                std::advance(replaceIter, capture->Index);
                capture->ReplaceWith(replaceIter->Get());

                iter = block->Insts.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }

    // remove inlined PushScopes/PopScopes
    for (auto &block : func->Blocks)
    {
        block->Insts.remove_if([&](const std::unique_ptr<IRInst> &inst) -> bool
        {
            if (inst->Is<ir::PopScope>())
            {
                return (!inst->As<ir::PopScope>()->Scope) ||
                    (unableToInline.find(inst->As<ir::PopScope>()->Scope->As<ir::PushScope>()) == unableToInline.end());
            }

            return false;
        });
    }

    for (auto &block : func->Blocks)
    {
        block->Insts.remove_if([&](const std::unique_ptr<IRInst> &inst) -> bool
        {
            if (inst->Is<ir::PushScope>())
            {
                if (unableToInline.find(inst->As<ir::PushScope>()) == unableToInline.end())
                {
                    inst->ReplaceWith(inst->As<ir::PushScope>()->UpperScope.Get());
                    return true;
                }
                else
                {
                    inst->As<ir::PushScope>()->Captured.clear();
                }
            }

            return false;
        });
    }
}

void Optimizer::ArgToLocalAllocaAndMoveCapture(IRFunc *func)
{
    std::map<IRInst *, IRBlock *> scopeToBlock;
    std::map<IRInst *, std::list<std::unique_ptr<IRInst>>::iterator> scopeToIter;
    size_t funcCapture = 0;

    scopeToBlock[nullptr] = func->Blocks.front().get();
    scopeToIter[nullptr] = std::find_if(
        func->Blocks.front()->Insts.begin(),
        func->Blocks.front()->Insts.end(),
        [&](const std::unique_ptr<IRInst> &inst) { return !inst->Is<ir::Alloca>(); }
    );

    for (auto &block : func->Blocks)
    {
        auto scope = block->Scope->As<ir::PushScope>();
        for (auto iter = block->Insts.begin();
            iter != block->Insts.end();
            ++iter)
        {
            if ((*iter)->Is<ir::PushScope>())
            {
                scopeToBlock[iter->get()] = block.get();
                auto currentIter = iter;
                scopeToIter[iter->get()] = ++currentIter;
                scope = iter->get()->As<ir::PushScope>();
            }
            else if ((*iter)->Is<ir::PopScope>())
            {
                scope = scope->UpperScope->As<ir::PushScope>();
            }
            else if (!scope && (*iter)->Is<ir::Capture>())
            {
                auto capture = (*iter)->As<ir::Capture>()->Index;
                if (capture + 1 > funcCapture)
                {
                    funcCapture = capture + 1;
                }
            }
        }
    }

    std::vector<IRInst*> args(func->Length, nullptr);
    {
        auto iter = scopeToIter[nullptr];
        auto block = scopeToBlock[nullptr];
        for (size_t i = 0; i < func->Length; ++i)
        {
            auto alloc = new ir::Alloca();
            iter = block->Insts.emplace(iter, alloc);
            ++iter;

            auto arg = new ir::Arg();
            arg->Index = i;
            iter = block->Insts.emplace(iter, arg);
            ++iter;

            auto load = new ir::Load();
            load->_Addr = arg;
            iter = block->Insts.emplace(iter, load);
            ++iter;

            auto store = new ir::Store();
            store->_Addr = alloc;
            store->_Value = load;
            iter = block->Insts.emplace(iter, store);
            ++iter;

            args[i] = alloc;
        }
    }

    std::map<IRInst *, std::vector<IRInst*>> captures;
    captures[nullptr] = std::vector<IRInst*>(funcCapture, nullptr);

    for (auto &block : func->Blocks)
    {
        auto scope = block->Scope->As<ir::PushScope>();
        for (auto iter = (block == func->Blocks.front()) ? scopeToIter[nullptr] : block->Insts.begin();
            iter != block->Insts.end();
            )
        {
            auto &inst = *iter;
            if (inst->Is<ir::PushScope>())
            {
                scope = inst->As<ir::PushScope>();
                ++iter;
            }
            else if (inst->Is<ir::PopScope>())
            {
                scope = scope->UpperScope->As<ir::PushScope>();
                ++iter;
            }
            else if (inst->Is<ir::Arg>())
            {
                auto arg = inst->As<ir::Arg>();
                arg->ReplaceWith(args[arg->Index]);
                iter = block->Insts.erase(iter);
            }
            else if (inst->Is<ir::Capture>())
            {
                auto capture = iter->get()->As<ir::Capture>();
                auto scopeIter = captures.find(scope);

                if (scopeIter == captures.end())
                {
                    auto pair = captures.emplace(
                        scope,
                        std::vector<IRInst *>(scope->Captured.size(), nullptr)
                    );

                    hydra_assert(pair.second, "insertion must succeed");
                    scopeIter = pair.first;
                }

                if (scopeIter->second[capture->Index])
                {
                    capture->ReplaceWith(scopeIter->second[capture->Index]);
                    iter = block->Insts.erase(iter);
                }
                else if (block.get() != scopeToBlock[scope] || iter != scopeToIter[scope])
                {
                    capture = iter->release()->As<ir::Capture>();
                    scopeIter->second[capture->Index] = capture;
                    iter = block->Insts.erase(iter);
                    auto &iterInScope = scopeToIter[scope];
                    iterInScope = scopeToBlock[scope]->Insts.emplace(
                        iterInScope,
                        capture
                    );
                    ++iterInScope;
                }
                else
                {
                    ++iter;
                }
            }
            else
            {
                ++iter;
            }
        }
    }
}

void Optimizer::MemToReg(IRFunc *func)
{
    std::vector<ir::Alloca *> allocaToPromote;
    std::set<ir::Alloca *> unableToPromote;

    for (auto &block : func->Blocks)
    {
        auto scope = block->Scope->As<ir::PushScope>();
        for (auto &inst : block->Insts)
        {
            if (inst->Is<ir::PushScope>())
            {
                scope = inst->As<ir::PushScope>();
            }
            else if (inst->Is<ir::PopScope>())
            {
                scope = scope->UpperScope->As<ir::PushScope>();
            }
            else if (inst->Is<ir::Alloca>())
            {
                allocaToPromote.push_back(inst->As<ir::Alloca>());
            }
            else if (inst->Is<ir::Func>())
            {
                for (auto &captured : inst->As<ir::Func>()->Captured)
                {
                    if (captured->Is<ir::Alloca>())
                    {
                        unableToPromote.insert(captured->As<ir::Alloca>());
                    }
                    else if (captured->Is<ir::Capture>())
                    {
                        auto captureInst = captured.Get();

                        while (captureInst->Is<ir::Capture>() &&
                            captureInst->As<ir::Capture>()->Scope)
                        {
                            auto captureIter = captureInst->As<ir::Capture>()->Scope
                                ->As<ir::PushScope>()->Captured.begin();
                            std::advance(captureIter, captureInst->As<ir::Capture>()->Index);
                            captureInst = captureIter->Get();
                        }

                        if (captureInst->Is<ir::Alloca>())
                        {
                            unableToPromote.insert(captureInst->As<ir::Alloca>());
                        }
                    }
                }
            }
            else if (inst->Is<ir::Arrow>())
            {
                for (auto &captured : inst->As<ir::Arrow>()->Captured)
                {
                    if (captured->Is<ir::Alloca>())
                    {
                        unableToPromote.insert(captured->As<ir::Alloca>());
                    }
                    else if (captured->Is<ir::Capture>())
                    {
                        auto captureInst = captured.Get();

                        while (captureInst->Is<ir::Capture>() &&
                            captureInst->As<ir::Capture>()->Scope)
                        {
                            auto captureIter = captureInst->As<ir::Capture>()->Scope
                                ->As<ir::PushScope>()->Captured.begin();
                            std::advance(captureIter, captureInst->As<ir::Capture>()->Index);
                            captureInst = captureIter->Get();
                        }

                        if (captureInst->Is<ir::Alloca>())
                        {
                            unableToPromote.insert(captureInst->As<ir::Alloca>());
                        }
                    }
                }
            }
        }
    }

    allocaToPromote.erase(
        std::remove_if(
            allocaToPromote.begin(),
            allocaToPromote.end(),
            [&](ir::Alloca *inst)
            {
                return unableToPromote.find(inst) != unableToPromote.end();
            }),
        allocaToPromote.end());

    for (auto allocaInst : allocaToPromote)
    {
        std::vector<IRInst *> lastValue(func->Blocks.size(), nullptr);
        std::vector<bool> hasPlaceholder(func->Blocks.size(), false);
        std::vector<bool> noValue(func->Blocks.size(), false);

        std::function<void(IRBlock *)> dfs = [&](IRBlock *block)
        {
            if (lastValue[block->Index])
            {
                return;
            }

            if (noValue[block->Index])
            {
                return;
            }

            IRInst *last = nullptr;

            if (block->Precedences.size() == 1)
            {
                last = lastValue[block->Precedences.front()->Index];
            }
            else if (block->Precedences.size() > 1)
            {
                bool hasNoValue = false;
                bool hasNotVisited = false;
                bool allSame = false;
                IRInst *value = nullptr;

                for (auto &precedence : block->Precedences)
                {
                    if (noValue[precedence->Index])
                    {
                        hasNoValue = true;
                        break;
                    }
                    else if (!lastValue[precedence->Index])
                    {
                        hasNotVisited = true;
                        break;
                    }
                }

                if (!hasNoValue && !hasNotVisited)
                {
                    value = lastValue[block->Precedences.front()->Index];
                    allSame = true;

                    for (auto &precedence : block->Precedences)
                    {
                        if (value != lastValue[precedence->Index])
                        {
                            allSame = false;
                            break;
                        }
                    }
                }

                if (hasNoValue)
                {
                    last = nullptr;
                }
                else if (allSame)
                {
                    hydra_assert(value, "must have a value here");
                    last = value;
                }
                else
                {
                    auto phi = new ir::Phi();
                    block->Insts.emplace_front(phi);

                    for (auto &precedence : block->Precedences)
                    {
                        if (lastValue[precedence->Index])
                        {
                            phi->Branches.emplace_back(precedence, lastValue[precedence->Index]);
                        }
                    }

                    last = phi;

                    hasPlaceholder[block->Index] = (phi->Branches.size() != block->Precedences.size());
                }
            }

            auto scope = block->Scope->As<ir::PushScope>();
            for (auto iter = block->Insts.begin(); iter != block->Insts.end(); )
            {
                auto inst = iter->get();

                if (inst->Is<ir::Load>())
                {
                    auto load = inst->As<ir::Load>();

                    if (load->_Addr.Get() == allocaInst)
                    {
                        hydra_assert(last, "must have an value here");
                        load->ReplaceWith(last);
                        iter = block->Insts.erase(iter);
                        continue;
                    }
                    else if (load->_Addr->Is<ir::Capture>())
                    {
                        IRInst *captured = load->_Addr.Get();

                        while (captured->Is<ir::Capture>() &&
                            captured->As<ir::Capture>()->Scope)
                        {
                            auto captureIter = captured->As<ir::Capture>()->Scope
                                ->As<ir::PushScope>()->Captured.begin();
                            std::advance(captureIter, captured->As<ir::Capture>()->Index);
                            captured = captureIter->Get();
                        }

                        if (captured->Is<ir::Alloca>() && captured->As<ir::Alloca>() == allocaInst)
                        {
                            hydra_assert(last, "must have an value here");
                            load->ReplaceWith(last);
                            iter = block->Insts.erase(iter);
                            continue;
                        }
                    }
                }
                else if (inst->Is<ir::Store>())
                {
                    auto store = inst->As<ir::Store>();

                    if (store->_Addr.Get() == allocaInst)
                    {
                        last = store->_Value.Get();
                        iter = block->Insts.erase(iter);
                        continue;
                    }
                    else if (store->_Addr->Is<ir::Capture>())
                    {
                        IRInst *captured = store->_Addr.Get();

                        while (captured->Is<ir::Capture>() &&
                            captured->As<ir::Capture>()->Scope)
                        {
                            auto captureIter = captured->As<ir::Capture>()->Scope
                                ->As<ir::PushScope>()->Captured.begin();
                            std::advance(captureIter, captured->As<ir::Capture>()->Index);
                            captured = captureIter->Get();
                        }

                        if (captured->Is<ir::Alloca>() && captured->As<ir::Alloca>() == allocaInst)
                        {
                            last = store->_Value.Get();
                            iter = block->Insts.erase(iter);
                            continue;
                        }
                    }
                }
                else
                {
                    if (inst->Is<ir::PushScope>())
                    {
                        scope = inst->As<ir::PushScope>();
                    }
                    else if (inst->Is<ir::PopScope>())
                    {
                        scope = scope->UpperScope->As<ir::PushScope>();
                    }
                }
                ++iter;
            }

            lastValue[block->Index] = last;
            if (!last)
            {
                noValue[block->Index] = true;
            }

            if (block->Consequent)
            {
                if (hasPlaceholder[block->Consequent->Index])
                {
                    auto front = block->Consequent->Insts.front().get();
                    hydra_assert(front->Is<ir::Phi>(), "front must be a phi");
                    front->As<ir::Phi>()->Branches.emplace_back(std::pair<IRBlock::Ref, IRInst::Ref>{
                        block,
                            last
                    });
                }
                else if (!lastValue[block->Consequent->Index])
                {
                    dfs(block->Consequent.Get());
                }
            }
            if (block->Alternate)
            {
                if (hasPlaceholder[block->Alternate->Index])
                {
                    auto front = block->Alternate->Insts.front().get();
                    hydra_assert(front->Is<ir::Phi>(), "front must be a phi");
                    front->As<ir::Phi>()->Branches.emplace_back(std::pair<IRBlock::Ref, IRInst::Ref>{
                        block,
                            last
                    });
                }
                else if (!lastValue[block->Alternate->Index])
                {
                    dfs(block->Alternate.Get());
                }
            }
        };

        dfs(func->Blocks.front().get());
    }

    for (auto &block : func->Blocks)
    {
        auto scope = block->Scope->As<ir::PushScope>();

        for (auto iter = block->Insts.begin();
            iter != block->Insts.end();)
        {
            auto inst = iter->get();
            if (inst->Is<ir::Alloca>() &&
                (unableToPromote.find(inst->As<ir::Alloca>()) == unableToPromote.end()))
            {
                iter = block->Insts.erase(iter);
            }
            else
            {
                if (inst->Is<ir::PushScope>())
                {
                    scope = inst->As<ir::PushScope>();
                }
                else if (inst->Is<ir::PopScope>())
                {
                    scope = scope->UpperScope->As<ir::PushScope>();
                }

                ++iter;
            }
        }
    }

    bool needMoreIteration = false;

    do
    {
        needMoreIteration = false;

        for (auto &block : func->Blocks)
        {
            for (auto iter = block->Insts.begin();
                iter != block->Insts.end();)
            {
                auto inst = iter->get();

                if (inst->Is<ir::Phi>())
                {
                    auto phi = inst->As<ir::Phi>();

                    if (phi->Branches.size() != block->Precedences.size())
                    {
                        iter = block->Insts.erase(iter);
                        needMoreIteration = true;
                        continue;
                    }

                    bool hasNull = false;
                    bool allSame = true;

                    auto otherValueIter = std::find_if(
                        phi->Branches.begin(),
                        phi->Branches.end(),
                        [&](const std::pair<IRBlock::Ref, IRInst::Ref> &pair)
                    {
                        if (pair.second.Get() == nullptr)
                        {
                            hasNull = true;
                        }

                        return pair.second.Get() != phi;
                    });

                    if (otherValueIter == phi->Branches.end())
                    {
                        iter = block->Insts.erase(iter);
                        needMoreIteration = true;
                        continue;
                    }

                    if (hasNull)
                    {
                        iter = block->Insts.erase(iter);
                        needMoreIteration = true;
                        continue;
                    }

                    auto otherValue = otherValueIter->second.Get();
                    while (++otherValueIter != phi->Branches.end())
                    {
                        if (otherValueIter->second.Get() != otherValue &&
                            otherValueIter->second.Get() != phi)
                        {
                            allSame = false;
                        }

                        if (otherValueIter->second.Get() == nullptr)
                        {
                            hasNull = true;
                            break;
                        }
                    }

                    if (hasNull)
                    {
                        iter = block->Insts.erase(iter);
                        needMoreIteration = true;
                        continue;
                    }

                    if (allSame)
                    {
                        phi->ReplaceWith(otherValue);
                        iter = block->Insts.erase(iter);
                        needMoreIteration = true;
                        continue;
                    }
                }

                ++iter;
            }
        }

    } while (needMoreIteration);
}

void Optimizer::CleanupBlocks(IRFunc *func)
{
    for (auto &block : func->Blocks)
    {
        if (block->Precedences.size() == 1 &&
            block->Precedences.front()->Condition.Get() == nullptr)
        {
            auto precedence = block->Precedences.front().Get();
            hydra_assert(precedence->Consequent.Get() == block.get(),
                "precedence->Consequent should be block");

            precedence->Insts.splice(
                precedence->Insts.end(),
                block->Insts);
            precedence->Condition = block->Condition;
            precedence->Consequent = block->Consequent;
            precedence->Alternate = block->Alternate;
            precedence->EndScope = block->EndScope;

            block->ReplaceWith(precedence);
        }
    }

    RemoveUnreferencedBlocks(func);
}

void Optimizer::LoopAnalyze(IRFunc *func)
{
    std::vector<bool> visited(func->Blocks.size(), false);
    std::vector<bool> inStack(func->Blocks.size(), false);
    std::vector<IRBlock *> stack;

    auto updateLoopHeader = [&](IRBlock *header)
    {
        for (auto iter = stack.rbegin(); iter != stack.rend(); ++iter)
        {
            if ((*iter)->LoopHeader)
            {
                if (DominatedBy(header, (*iter)->LoopHeader.Get()))
                {
                    (*iter)->LoopHeader = header;
                }
            }
            else
            {
                (*iter)->LoopHeader = header;
            }

            if ((*iter) == header)
            {
                break;
            }
        }
    };

    std::function<void(IRBlock *)> dfs = [&](IRBlock *block)
    {
        visited[block->Index] = true;
        inStack[block->Index] = true;
        stack.push_back(block);

        if (block->Consequent)
        {
            if (inStack[block->Consequent->Index])
            {
                auto header = block->Consequent.Get();
                updateLoopHeader(header);
            }
            else if (visited[block->Consequent->Index])
            {
                auto header = block->Consequent->LoopHeader.Get();
                if (header)
                {
                    updateLoopHeader(header);
                }
            }
            else
            {
                dfs(block->Consequent.Get());
            }
        }

        if (block->Alternate)
        {
            if (inStack[block->Alternate->Index])
            {
                auto header = block->Alternate.Get();
                updateLoopHeader(header);
            }
            else if (visited[block->Alternate->Index])
            {
                auto header = block->Alternate->LoopHeader.Get();
                if (header)
                {
                    updateLoopHeader(header);
                }
            }
            else
            {
                dfs(block->Alternate.Get());
            }
        }

        inStack[block->Index] = false;
        stack.pop_back();
    };

    dfs(func->Blocks.front().get());

    std::fill(visited.begin(), visited.end(), false);
    std::function<void(IRBlock *)> dfsToUpdateDepth = [&](IRBlock *block)
    {
        visited[block->Index] = true;
        if (block->LoopHeader)
        {
            if (block->LoopHeader.Get() == block)
            {
                auto iter = std::find_if(
                    block->Precedences.begin(),
                    block->Precedences.end(),
                    [&](const IRBlock::Ref &prec)
                    {
                        return !DominatedBy(prec.Get(), block);
                    }
                );

                hydra_assert(iter != block->Precedences.end(),
                    "Loop must have a previous");

                block->LoopPrev = iter->Get();

                iter = std::find_if(
                    block->Precedences.begin(),
                    block->Precedences.end(),
                    [&](const IRBlock::Ref &prec)
                    {
                        return (prec.Get() != block->LoopPrev.Get()) &&
                            !DominatedBy(prec.Get(), block);
                    }
                );
                hydra_assert(iter == block->Precedences.end(),
                    "Loop must have only one previous");

                hydra_assert(visited[block->LoopPrev->Index],
                    "loop previous must be visited");
                block->LoopDepth = block->LoopPrev->LoopDepth + 1;
            }
            else
            {
                block->LoopPrev = block->LoopHeader->LoopPrev;
                block->LoopDepth = block->LoopHeader->LoopDepth;
            }
        }
        else
        {
            block->LoopDepth = 0;
        }

        if (block->Consequent && !visited[block->Consequent->Index])
        {
            dfsToUpdateDepth(block->Consequent.Get());
        }

        if (block->Alternate && !visited[block->Alternate->Index])
        {
            dfsToUpdateDepth(block->Alternate.Get());
        }
    };
    dfsToUpdateDepth(func->Blocks.front().get());
}

void Optimizer::RemoveMove(IRFunc *func)
{
    for (auto &block : func->Blocks)
    {
        block->Insts.remove_if([&](const std::unique_ptr<IRInst> &inst)
        {
            if (inst->Is<ir::Move>())
            {
                inst->ReplaceWith(inst->As<ir::Move>()->_Other.Get());
                return true;
            }

            return false;
        });
    }
}

void Optimizer::RemoveLoopInvariant(IRFunc *func)
{
    std::map<IRInst *, IRBlock *> instToBlock;

    for (auto &block : func->Blocks)
    {
        for (auto &inst : block->Insts)
        {
            instToBlock[inst.get()] = block.get();
        }
    }

    std::vector<bool> visited(func->Blocks.size(), false);
    std::function<void(IRBlock *)> dfs = [&](IRBlock *block)
    {
        visited[block->Index] = true;

        if (block->LoopHeader)
        {
            block->Insts.remove_if([&](std::unique_ptr<IRInst> &inst)
            {
                switch (inst->GetType())
                {
                case UNDEFINED:
                case NULL:
                case TRUE:
                case FALSE:
                case NUMBER:
                case STRING:
                case REGEX:
                case THIS:
                {
                    auto dst = block;
                    while (dst->LoopHeader)
                    {
                        dst = dst->LoopPrev.Get();
                    }
                    hydra_assert(dst != block, "must have a LoopPrev");

                    instToBlock[inst.get()] = dst;
                    dst->Insts.emplace_back(inst.release());
                    return true;
                }
                case ADD:
                case SUB:
                case MUL:
                case DIV:
                case MOD:
                case BAND:
                case BOR:
                case BXOR:
                case SLL:
                case SRL:
                case SRR:
                case EQ:
                case EQQ:
                case NE:
                case NEE:
                case GT:
                case GE:
                case LT:
                case LE:
                case IN:
                case INSTANCEOF:
                {
                    auto binary = inst->As<ir::Binary>();
                    auto leftBlock = instToBlock[binary->_A.Get()];
                    auto rightBlock = instToBlock[binary->_B.Get()];

                    IRBlock *innerLoop;
                    if (leftBlock->LoopDepth < rightBlock->LoopDepth)
                    {
                        innerLoop = rightBlock->LoopHeader.Get();
                    }
                    else
                    {
                        innerLoop = leftBlock->LoopHeader.Get();
                    }

                    auto dst = block;
                    while (dst->LoopHeader.Get() != innerLoop)
                    {
                        dst = dst->LoopPrev.Get();
                    }

                    if (dst != block)
                    {
                        instToBlock[inst.get()] = dst;
                        dst->Insts.emplace_back(inst.release());
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }
                case BNOT:
                case LNOT:
                case TYPEOF:
                {
                    auto unary = inst->As<ir::Unary>();
                    IRBlock *innerLoop = instToBlock[unary->_A.Get()];
                    auto dst = block;
                    while (dst->LoopHeader.Get() != innerLoop)
                    {
                        dst = dst->LoopPrev.Get();
                    }

                    if (dst != block)
                    {
                        instToBlock[inst.get()] = dst;
                        dst->Insts.emplace_back(inst.release());
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }
                default:
                    return false;
                }
            });
        }

        if (block->Consequent && !visited[block->Consequent->Index])
        {
            dfs(block->Consequent.Get());
        }

        if (block->Alternate && !visited[block->Alternate->Index])
        {
            dfs(block->Alternate.Get());
        }
    };

    dfs(func->Blocks.front().get());
}

void Optimizer::RemoveUnreferencedBlocks(IRFunc *func)
{
    std::vector<bool> referenced(func->Blocks.size(), false);

    size_t index = 0;
    for (auto &block : func->Blocks)
    {
        block->Index = index++;
    }

    std::function<void(IRBlock *)> dfs = [&](IRBlock *block)
    {
        referenced[block->Index] = true;

        if (block->Consequent && !referenced[block->Consequent->Index])
        {
            dfs(block->Consequent.Get());
        }

        if (block->Alternate && !referenced[block->Alternate->Index])
        {
            dfs(block->Alternate.Get());
        }
    };

    dfs(func->Blocks.front().get());

    func->Blocks.remove_if([&](const std::unique_ptr<IRBlock> &block)
    {
        return !referenced[block->Index];
    });

    index = 0;
    for (auto &block : func->Blocks)
    {
        block->Index = index++;

        for (auto &inst : block->Insts)
        {
            if (inst->Is<ir::Phi>())
            {
                auto phi = inst->As<ir::Phi>();
                phi->Branches.remove_if([](const std::pair<IRBlock::Ref, IRInst::Ref> &pair)
                {
                    return pair.first.Get() == nullptr;
                });
            }
        }
    }
}

bool Optimizer::DominatedBy(IRBlock *a, IRBlock *b)
{
    while (a)
    {
        if (a->Dominator.Get() == b)
        {
            return true;
        }

        a = a->Dominator.Get();
    }
    return false;
}

#pragma pop_macro("THIS")
#pragma pop_macro("IN")
#pragma pop_macro("FALSE")
#pragma pop_macro("TRUE")
#pragma pop_macro("NULL")

} // namespace vm
} // namespace hydra