#include "Compile.h"

#include "IR.h"
#include "IRInsts.h"
#include "runtime/Semantic.h"

#include <vector>

namespace hydra
{
namespace vm
{

#define LOAD_REG(_reg, _SrcReg)                                 \
    mov(_reg, ptr[rdx + Scope::OffsetRegs()]);                  \
    add(_reg, static_cast<u32>(runtime::Array::OffsetTable())); \
    mov(_reg, ptr[_reg + 8 * (_SrcReg)->Index]);

#define STORE_REG(_reg, _helper, _DstReg)                           \
    mov(_helper, ptr[rdx + Scope::OffsetRegs()]);                   \
    add(_helper, static_cast<u32>(runtime::Array::OffsetTable()));  \
    mov(ptr[_helper + 8 * (_DstReg)->Index], _reg);

#define SET_RESULT(_reg, _src_reg)                              \
    mov(_reg, ptr[rdx + Scope::OffsetRegs()]);                  \
    add(_reg, static_cast<u32>(runtime::Array::OffsetTable())); \
    mov(ptr[_reg + 8 * inst->Index], _src_reg);

#define RETVAL_REG(_reg)                                        \
    mov(_reg, ptr[rdx + Scope::OffsetRegs()]);                  \
    add(_reg, static_cast<u32>(runtime::Array::OffsetTable())); \
    lea(_reg, ptr[_reg + 8 * inst->Index]);


GeneratedCode BaselineCompileTask::Compile(size_t &registerCount)
{
    registerCount = IR->UpdateIndex();

    std::vector<Label> labels(IR->Blocks.size());
    Label returnPoint, throwPoint;

    mov(ptr[rsp + 32], r9);
    mov(ptr[rsp + 24], r8);
    mov(ptr[rsp + 16], rdx);
    mov(ptr[rsp + 8], rcx);

    push(rbp);
    push(rbx);
    push(r10);
    push(r15);

    sub(rsp, 56);
    lea(rbp, ptr[rsp + 88]);

    mov(r15, cexpr::Mask(0, 48));

    for (auto &block : IR->Blocks)
    {
        L(labels[block->Index]);

        for (auto &inst : block->Insts)
        {
            switch (inst->GetType())
            {
            case RETURN:
            {
                LOAD_REG(rax, inst->As<ir::Return>()->_Value);
                jmp(returnPoint, T_NEAR);
                break;
            }
            case LOAD:
            {
                LOAD_REG(rax, inst->As<ir::Load>()->_Addr);
                and(rax, r15);
                mov(rax, ptr[rax]);
                SET_RESULT(rbx, rax);
                break;
            }
            case STORE:
            {
                LOAD_REG(rax, inst->As<ir::Store>()->_Addr);
                LOAD_REG(rbx, inst->As<ir::Store>()->_Value);
                and(rax, r15);
                mov(ptr[rax], rbx);
                break;
            }
            case GET_ITEM:
            {
                if (inst->As<ir::GetItem>()->_Key->Is<ir::String>())
                {
                    // inline cached version
                    inLocalLabel();

                    LOAD_REG(rax, inst->As<ir::GetItem>()->_Obj);

                    // make sure it is an object
                    mov(rbx, rax);
                    shr(rbx, 48);
                    cmp(rbx, 0xFFFA);
                    jne("slowPath");

                    // detect cached
                    and(rax, r15);
                    L("cached");
                    mov(rbx, (u64)0x0123456789ABCDEFull);
                    cmp(rbx, ptr[rax + runtime::JSObject::OffsetKlass()]);
                    jne("slowPath");

                    // load by index
                    mov(rbx, (u64)0x0123456789ABCDEFull);
                    mov(rax, ptr[rax + runtime::JSObject::OffsetTable()]);
                    mov(rax, ptr[rax + rbx]);
                    RETVAL_REG(rbx);
                    mov(ptr[rbx], rax);
                    jmp("finish");

                    L("slowPath");
                    LOAD_REG(rax, inst->As<ir::GetItem>()->_Obj);
                    LOAD_REG(rbx, inst->As<ir::GetItem>()->_Key);

                    // &error
                    mov(ptr[rsp + 40], r9);

                    // &retVal
                    RETVAL_REG(r9);
                    mov(ptr[rsp + 32], r9);

                    // fixup
                    mov(r9, "cached");

                    // key
                    mov(r8, rbx);

                    // object
                    mov(rdx, rax);

                    mov(rax, reinterpret_cast<u64>(runtime::semantic::ObjectGetAndFixCache));
                    call(rax);

                    mov(r9, ptr[rbp + 32]);
                    mov(r8, ptr[rbp + 24]);
                    mov(rdx, ptr[rbp + 16]);
                    mov(rcx, ptr[rbp + 8]);

                    test(rax, rax);
                    jz(throwPoint, T_NEAR);

                    L("finish");
                    outLocalLabel();
                }
                else
                {
                    LOAD_REG(rax, inst->As<ir::GetItem>()->_Obj);
                    LOAD_REG(rbx, inst->As<ir::GetItem>()->_Key);

                    // &error
                    mov(ptr[rsp + 32], r9);

                    // &retVal
                    RETVAL_REG(r9);

                    // key
                    mov(r8, rbx);

                    // object
                    mov(rdx, rax);

                    mov(rax, reinterpret_cast<u64>(runtime::semantic::ObjectGet));
                    call(rax);

                    mov(r9, ptr[rbp + 32]);
                    mov(r8, ptr[rbp + 24]);
                    mov(rdx, ptr[rbp + 16]);
                    mov(rcx, ptr[rbp + 8]);

                    test(rax, rax);
                    jz(throwPoint, T_NEAR);
                }

                break;
            }
            case SET_ITEM:
            {
                LOAD_REG(rax, inst->As<ir::SetItem>()->_Obj);
                LOAD_REG(rbx, inst->As<ir::SetItem>()->_Key);
                LOAD_REG(r10, inst->As<ir::SetItem>()->_Value);

                // &error
                mov(ptr[rsp + 32], r9);

                // value
                mov(r9, r10);

                // key
                mov(r8, rbx);

                // object
                mov(rdx, rax);

                mov(rax, reinterpret_cast<u64>(runtime::semantic::ObjectSet));
                call(rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(rdx, ptr[rbp + 16]);
                mov(rcx, ptr[rbp + 8]);

                test(rax, rax);
                jz(throwPoint, T_NEAR);

                break;
            }
            case DEL_ITEM:
            {
                LOAD_REG(rax, inst->As<ir::DelItem>()->_Obj);
                LOAD_REG(rbx, inst->As<ir::DelItem>()->_Key);

                // key
                mov(r8, rbx);

                // object
                mov(rdx, rax);

                mov(rax, reinterpret_cast<u64>(runtime::semantic::ObjectDelete));
                call(rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(rdx, ptr[rbp + 16]);
                mov(rcx, ptr[rbp + 8]);

                test(rax, rax);
                jz(throwPoint, T_NEAR);

                break;
            }
            case NEW:
            {
                LOAD_REG(rax, inst->As<ir::New>()->_Callee);
                LOAD_REG(rbx, inst->As<ir::New>()->_Args);

                // &error
                // push(r9);
                mov(ptr[rsp + 32], r9);

                // &retVal
                RETVAL_REG(r9);

                // *arguments
                mov(r8, rbx);
                and(r8, r15);

                // constructor
                mov(rdx, rax);

                mov(rax, reinterpret_cast<u64>(runtime::semantic::NewObject));
                call(rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(rdx, ptr[rbp + 16]);
                mov(rcx, ptr[rbp + 8]);

                test(rax, rax);
                jz(throwPoint, T_NEAR);

                break;
            }
            case CALL:
            {
                LOAD_REG(rax, inst->As<ir::Call>()->_Callee);
                LOAD_REG(r10, inst->As<ir::Call>()->_ThisArg);
                LOAD_REG(rbx, inst->As<ir::Call>()->_Args);

                // &error
                mov(ptr[rsp + 40], r9);

                // &retVal
                RETVAL_REG(r9);
                mov(ptr[rsp + 32], r9);

                // *arguments
                mov(r9, rbx);
                and(r9, r15);

                // thisArg
                mov(r8, r10);

                // callee
                mov(rdx, rax);

                mov(rax, reinterpret_cast<u64>(runtime::semantic::Call));
                call(rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(rdx, ptr[rbp + 16]);
                mov(rcx, ptr[rbp + 8]);

                test(rax, rax);
                jz(throwPoint, T_NEAR);

                break;
            }
            case GET_GLOBAL:
            {
                // &retVal
                RETVAL_REG(r8);

                // name
                mov(rdx, reinterpret_cast<uintptr_t>(inst->As<ir::GetGlobal>()->Name));

                mov(rax, reinterpret_cast<u64>(runtime::semantic::GetGlobal));
                call(rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(rdx, ptr[rbp + 16]);
                mov(rcx, ptr[rbp + 8]);

                test(rax, rax);
                jz(throwPoint, T_NEAR);

                break;
            }
            case SET_GLOBAL:
            {
                LOAD_REG(rax, inst->As<ir::SetGlobal>()->_Value);

                // value
                mov(r8, rax);

                // name
                mov(rdx, reinterpret_cast<uintptr_t>(inst->As<ir::SetGlobal>()->Name));

                mov(rax, reinterpret_cast<u64>(runtime::semantic::SetGlobal));
                call(rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(rdx, ptr[rbp + 16]);
                mov(rcx, ptr[rbp + 8]);

                test(rax, rax);
                jz(throwPoint, T_NEAR);

                break;
            }
            case UNDEFINED:
            {
                mov(rax, runtime::JSValue::UNDEFINED_PAYLOAD);
                SET_RESULT(rbx, rax);
                break;
            }
#pragma push_macro("NULL")
#undef NULL
            case NULL:
#pragma pop_macro("NULL")
            {
                mov(rax, runtime::JSValue::FromObject(nullptr).Payload);
                SET_RESULT(rbx, rax);
                break;
            }
#pragma push_macro("TRUE")
#undef TRUE
            case TRUE:
#pragma pop_macro("TRUE")
            {
                mov(rax, runtime::JSValue::FromBoolean(true).Payload);
                SET_RESULT(rbx, rax);
                break;
            }
#pragma push_macro("FALSE")
#undef FALSE
            case FALSE:
#pragma pop_macro("FALSE")
            {
                mov(rax, runtime::JSValue::FromBoolean(false).Payload);
                SET_RESULT(rbx, rax);
                break;
            }
            case NUMBER:
            {
                double intpart;
                double value = inst->As<ir::Number>()->Value;
                if (value < (1ll << 47) && (value > -(1ll << 47)) &&
                    std::modf(value, &intpart) == 0.0)
                {
                    mov(rax, runtime::JSValue::FromSmallInt(static_cast<i64>(intpart)).Payload);
                }
                else
                {
                    mov(rax, runtime::JSValue::FromNumber(value).Payload);
                }
                SET_RESULT(rbx, rax);
                break;
            }
            case STRING:
            {
                mov(rax, runtime::JSValue::FromString(inst->As<ir::String>()->Value).Payload);
                SET_RESULT(rbx, rax);
                break;
            }
            case REGEX:
            {
                hydra_trap("Not supporting regex");
                break;
            }
            case OBJECT:
            {
                // &error
                mov(ptr[rsp + 32], r9);

                // &retVal
                RETVAL_REG(r9);

                // inst
                mov(r8, reinterpret_cast<uintptr_t>(inst.get()));

                mov(rax, reinterpret_cast<u64>(runtime::semantic::NewObjectWithInst));
                call(rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(rdx, ptr[rbp + 16]);
                mov(rcx, ptr[rbp + 8]);

                test(rax, rax);
                jz(throwPoint, T_NEAR);

                break;
            }
            case ARRAY:
            {
                // &error
                mov(ptr[rsp + 32], r9);

                // &retVal
                RETVAL_REG(r9);

                // inst
                mov(r8, reinterpret_cast<uintptr_t>(inst.get()));

                mov(rax, reinterpret_cast<u64>(runtime::semantic::NewArrayWithInst));
                call(rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(rdx, ptr[rbp + 16]);
                mov(rcx, ptr[rbp + 8]);

                test(rax, rax);
                jz(throwPoint, T_NEAR);

                break;
            }
            case FUNC:
            {
                auto funcInst = inst->As<ir::Func>();
                if (funcInst->FuncId < IR->Module->Functions.size())
                {
                    funcInst->FuncPtr = IR->Module->Functions[funcInst->FuncId].get();
                }

                // &error
                mov(ptr[rsp + 32], r9);

                // &retVal
                RETVAL_REG(r9);

                // inst
                mov(r8, reinterpret_cast<uintptr_t>(inst.get()));

                mov(rax, reinterpret_cast<u64>(runtime::semantic::NewFuncWithInst));
                call(rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(rdx, ptr[rbp + 16]);
                mov(rcx, ptr[rbp + 8]);

                test(rax, rax);
                jz(throwPoint, T_NEAR);

                break;
            }
            case ARROW:
            {
                auto funcInst = inst->As<ir::Arrow>();
                if (funcInst->FuncId < IR->Module->Functions.size())
                {
                    funcInst->FuncPtr = IR->Module->Functions[funcInst->FuncId].get();
                }

                // &error
                mov(ptr[rsp + 32], r9);

                // &retVal
                RETVAL_REG(r9);

                // inst
                mov(r8, reinterpret_cast<uintptr_t>(inst.get()));

                mov(rax, reinterpret_cast<u64>(runtime::semantic::NewArrowWithInst));
                call(rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(rdx, ptr[rbp + 16]);
                mov(rcx, ptr[rbp + 8]);

                test(rax, rax);
                jz(throwPoint, T_NEAR);

                break;
            }

#define CASE_BINARY(BIN, func)                                              \
            case BIN:                                                       \
            {                                                               \
                LOAD_REG(rax, inst->As<ir::Binary>()->_A);                  \
                LOAD_REG(rbx, inst->As<ir::Binary>()->_B);                  \
                                                                            \
                mov(ptr[rsp + 32], r9);                                     \
                                                                            \
                RETVAL_REG(r9);                                             \
                                                                            \
                mov(r8, rbx);                                               \
                                                                            \
                mov(rdx, rax);                                              \
                                                                            \
                mov(rax, reinterpret_cast<u64>(runtime::semantic::func));   \
                call(rax);                                                  \
                                                                            \
                mov(r9, ptr[rbp + 32]);                                     \
                mov(r8, ptr[rbp + 24]);                                     \
                mov(rdx, ptr[rbp + 16]);                                    \
                mov(rcx, ptr[rbp + 8]);                                     \
                                                                            \
                test(rax, rax);                                             \
                jz(throwPoint, T_NEAR);                                     \
                                                                            \
                break;                                                      \
            }
            CASE_BINARY(ADD, OpAdd);
            CASE_BINARY(SUB, OpSub);
            CASE_BINARY(MUL, OpMul);
            CASE_BINARY(DIV, OpDiv);
            CASE_BINARY(MOD, OpMod);
            CASE_BINARY(BAND, OpBand);
            CASE_BINARY(BOR, OpBor);
            CASE_BINARY(SLL, OpSll);
            CASE_BINARY(SRL, OpSrl);
            CASE_BINARY(SRR, OpSrr);
            CASE_BINARY(EQ, OpEq);
            CASE_BINARY(EQQ, OpEqq);
            CASE_BINARY(NE, OpNe);
            CASE_BINARY(NEE, OpNee);
            CASE_BINARY(LT, OpLt);
            CASE_BINARY(LE, OpLe);
            CASE_BINARY(GT, OpGt);
            CASE_BINARY(GE, OpGe);
#pragma push_macro("IN")
#undef IN
            CASE_BINARY(IN, OpIn);
#pragma pop_macro("IN")
            CASE_BINARY(INSTANCEOF, OpInstanceOf);

#define CASE_UNARY(UNA, func)                                               \
            case UNA:                                                       \
            {                                                               \
                LOAD_REG(rax, inst->As<ir::Unary>()->_A);                   \
                                                                            \
                RETVAL_REG(r8);                                             \
                                                                            \
                mov(rdx, rax);                                              \
                                                                            \
                mov(rax, reinterpret_cast<u64>(runtime::semantic::func));   \
                call(rax);                                                  \
                                                                            \
                mov(r9, ptr[rbp + 32]);                                     \
                mov(r8, ptr[rbp + 24]);                                     \
                mov(rdx, ptr[rbp + 16]);                                    \
                mov(rcx, ptr[rbp + 8]);                                     \
                                                                            \
                test(rax, rax);                                             \
                jz(throwPoint, T_NEAR);                                     \
                                                                            \
                break;                                                      \
            }

            CASE_UNARY(BNOT, OpBnot);
            CASE_UNARY(LNOT, OpLnot);
            CASE_UNARY(TYPEOF, OpTypeOf);

            case PUSH_SCOPE:
            {
                // inst
                mov(r8, reinterpret_cast<uintptr_t>(inst.get()));

                mov(rax, reinterpret_cast<u64>(runtime::semantic::NewScope));
                call(rax);

                mov(rdx, rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(ptr[rbp + 16], rdx);
                mov(rcx, ptr[rbp + 8]);

                break;
            }
            case POP_SCOPE:
            {
                for (size_t i = 0; i < inst->As<ir::PopScope>()->Count; ++i)
                {
                    mov(rdx, ptr[rdx + Scope::OffsetUpper()]);
                }
                mov(ptr[rbp + 16], rdx);
                break;
            }
            case ALLOCA:
            {
                mov(rax, reinterpret_cast<u64>(Scope::AllocateStatic));
                call(rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(rdx, ptr[rbp + 16]);
                mov(rcx, ptr[rbp + 8]);

                SET_RESULT(rbx, rax);

                break;
            }
            case ARG:
            {
                mov(rax, ptr[rdx + Scope::OffsetArguments()]);
                add(rax, static_cast<u32>(runtime::Array::OffsetTable()));
                lea(rax, ptr[rax + 8 * inst->As<ir::Arg>()->Index]);
                SET_RESULT(rbx, rax);
                break;
            }
            case CAPTURE:
            {
                mov(rax, ptr[rdx + Scope::OffsetCaptured()]);
                add(rax, static_cast<u32>(runtime::RangeArray::OffsetTable()));
                mov(rax, ptr[rax + 8 * inst->As<ir::Capture>()->Index]);
                SET_RESULT(rbx, rax);
                break;
            }
#pragma push_macro("THIS")
#undef THIS
            case THIS:
#pragma pop_macro("THIS")
            {
                mov(rax, ptr[rdx + Scope::OffsetThisArg()]);
                SET_RESULT(rbx, rax);
                break;
            }
            case ARGUMENTS:
            {
                RETVAL_REG(r8);

                mov(rax, reinterpret_cast<u64>(runtime::semantic::GetArguments));
                call(rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(rdx, ptr[rbp + 16]);
                mov(rcx, ptr[rbp + 8]);

                test(rax, rax);
                jz(throwPoint, T_NEAR);
                break;
            }
            case MOVE:
            {
                LOAD_REG(rax, inst->As<ir::Move>()->_Other);
                SET_RESULT(rbx, rax);
                break;
            }
            case PHI:
            {
                break;
            }
            default:
                hydra_trap("Unknown inst");
            }
        }

        // resolving phi
        if (block->Consequent)
        {
            for (auto &ref : block->Consequent->Insts)
            {
                if (!ref->Is<ir::Phi>())
                {
                    break;
                }

                auto phi = ref->As<ir::Phi>();

                auto pos = std::find_if(
                    phi->Branches.begin(),
                    phi->Branches.end(),
                    [&](const std::pair<IRBlock::Ref, IRInst::Ref> &pair)
                    {
                        return pair.first.Get() == block.get();
                    }
                );

                hydra_assert(pos != phi->Branches.end(),
                    "Phi not containing this block");

                LOAD_REG(rax, pos->second);
                STORE_REG(rax, rbx, phi);
            }
        }

        if (block->Alternate)
        {
            for (auto &ref : block->Alternate->Insts)
            {
                if (!ref->Is<ir::Phi>())
                {
                    break;
                }

                auto phi = ref->As<ir::Phi>();

                auto pos = std::find_if(
                    phi->Branches.begin(),
                    phi->Branches.end(),
                    [&](const std::pair<IRBlock::Ref, IRInst::Ref> &pair)
                    {
                        return pair.first.Get() == block.get();
                    }
                );

                hydra_assert(pos != phi->Branches.end(),
                    "Phi not containing this block");

                LOAD_REG(rax, pos->second);
                STORE_REG(rax, rbx, phi);
            }
        }

        if (block->Condition)
        {
                LOAD_REG(rax, block->Condition);

                mov(rcx, rax);

                mov(rax, reinterpret_cast<u64>(runtime::semantic::ToBoolean));
                call(rax);

                mov(r9, ptr[rbp + 32]);
                mov(r8, ptr[rbp + 24]);
                mov(rdx, ptr[rbp + 16]);
                mov(rcx, ptr[rbp + 8]);

                test(al, al);
                je(labels[block->Alternate->Index], T_NEAR);
                jmp(labels[block->Consequent->Index], T_NEAR);
        }
        else if (block->Consequent)
        {
            jmp(labels[block->Consequent->Index], T_NEAR);
        }
    }

    L(returnPoint);
    mov(ptr[r8], rax);
    mov(rax, 1);
    add(rsp, 56);
    pop(r15);
    pop(r10);
    pop(rbx);
    pop(rbp);
    ret();

    L(throwPoint);
    mov(rax, 0);
    add(rsp, 56);
    pop(r15);
    pop(r10);
    pop(rbx);
    pop(rbp);
    ret();

    return GetCode();
}

} // namespace vm
} // namespace hydra