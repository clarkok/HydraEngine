#include "Optimizer.h"

#include <stack>
#include <queue>
#include <map>

namespace hydra
{
namespace vm
{

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
        }
        );

        if (returnIter != block->Insts.end())
        {
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
    std::list<std::unique_ptr<IRInst>>::iterator entryIterator = func->Blocks.front()->Insts.begin();

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

                        if (!currentScope)
                        {
                            break;
                        }

                        auto captureIter = currentScope->Captured.begin();
                        std::advance(captureIter, captureInst->Index);
                        inst = captureIter->Get();

                        currentScope = currentScope->UpperScope->As<ir::PushScope>();
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

                        if (!currentScope)
                        {
                            break;
                        }

                        auto captureIter = currentScope->Captured.begin();
                        std::advance(captureIter, captureInst->Index);
                        inst = captureIter->Get();

                        currentScope = currentScope->UpperScope->As<ir::PushScope>();
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
            else if (inlinable && inst->Is<ir::Capture>())
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
            }

            return false;
        });
    }
}

void Optimizer::MemToReg(IRFunc *func)
{
    std::vector<ir::Alloca *> allocaToPromote;
    std::set<ir::Alloca *> unableToPromote;

    for (auto &block : func->Blocks)
    {
        for (auto &inst : block->Insts)
        {
            if (inst->Is<ir::Alloca>())
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
                        auto *capture = load->_Addr->As<ir::Capture>();
                        IRInst *captured = nullptr;
                        auto currentScope = scope;

                        do
                        {
                            if (!currentScope)
                            {
                                break;
                            }
                            auto captureIter = currentScope->Captured.begin();
                            std::advance(captureIter, capture->Index);
                            captured = captureIter->Get();
                            currentScope = currentScope->UpperScope->As<ir::PushScope>();
                        } while (captured->Is<ir::Capture>());

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
                        auto *capture = store->_Addr->As<ir::Capture>();
                        IRInst *captured = nullptr;
                        auto currentScope = scope;

                        do
                        {
                            if (!currentScope)
                            {
                                break;
                            }
                            auto captureIter = currentScope->Captured.begin();
                            std::advance(captureIter, capture->Index);
                            captured = captureIter->Get();
                            currentScope = currentScope->UpperScope->As<ir::PushScope>();
                        } while (captured->Is<ir::Capture>());

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

    std::map<ir::PushScope *, std::vector<size_t> > removedCapture;

    for (auto &block : func->Blocks)
    {
        for (auto &inst : block->Insts)
        {
            if (inst->Is<ir::PushScope>())
            {
                auto scope = inst->As<ir::PushScope>();
                std::vector<size_t> removed;

                size_t index = 0;
                for (auto &captured : scope->Captured)
                {
                    IRInst *capturedInst = captured.Get();
                    auto currentScope = scope->UpperScope->As<ir::PushScope>();
                    while (currentScope && capturedInst->Is<ir::Capture>())
                    {
                        auto captureIter = currentScope->Captured.begin();
                        std::advance(captureIter, capturedInst->As<ir::Capture>()->Index);
                        capturedInst = captureIter->Get();
                        currentScope = currentScope->UpperScope->As<ir::PushScope>();
                    }

                    if (capturedInst->Is<ir::Alloca>() &&
                        (unableToPromote.find(capturedInst->As<ir::Alloca>()) == unableToPromote.end()))
                    {
                        removed.push_back(index);
                    }
                    ++index;
                }

                if (removed.size())
                {
                    removedCapture[scope] = removed;
                }
            }
        }
    }

    for (auto &block : func->Blocks)
    {
        for (auto &inst : block->Insts)
        {
            if (inst->Is<ir::PushScope>())
            {
                auto scope = inst->As<ir::PushScope>();
                auto iter = removedCapture.find(scope);
                if (iter != removedCapture.end())
                {
                    size_t index = 0;
                    size_t removed = 0;
                    scope->Captured.remove_if([&](const IRInst::Ref &)
                    {
                        if (index == iter->second[removed])
                        {
                            ++removed;
                            ++index;
                            return true;
                        }
                        else
                        {
                            ++index;
                            return false;
                        }
                    });
                }
            }
        }
    }

    for (auto &block : func->Blocks)
    {
        auto scope = block->Scope->As<ir::PushScope>();
        bool needUpdate = scope && removedCapture.find(scope) != removedCapture.end();

        for (auto iter = block->Insts.begin();
            iter != block->Insts.end();)
        {
            auto inst = iter->get();
            if (inst->Is<ir::Alloca>() &&
                (unableToPromote.find(inst->As<ir::Alloca>()) == unableToPromote.end()))
            {
                iter = block->Insts.erase(iter);
            }
            else if (needUpdate && inst->Is<ir::Capture>())
            {
                bool needRemove = false;
                int delta = 0;
                auto capture = inst->As<ir::Capture>();

                for (auto removed : removedCapture[scope])
                {
                    if (removed == capture->Index)
                    {
                        needRemove = true;
                        break;
                    }
                    else if (removed < capture->Index)
                    {
                        ++delta;
                    }
                    else
                    {
                        break;
                    }
                }

                if (needRemove)
                {
                    iter = block->Insts.erase(iter);
                }
                else
                {
                    capture->Index -= delta;
                    ++iter;
                }
            }
            else
            {
                if (inst->Is<ir::PushScope>())
                {
                    scope = inst->As<ir::PushScope>();
                    needUpdate = removedCapture.find(scope) != removedCapture.end();
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

} // namespace vm
} // namespace hydra