#include "Compile.h"

#include "runtime/Semantic.h"

#include <vector>

namespace hydra
{
namespace vm
{

#define LOAD_REG(_reg, _SrcReg)                 \
    mov(_reg, ptr[rdx + Scope::OffsetRegs()]);  \
    add(_reg, runtime::Array::OffsetTable());   \
    mov(_reg, ptr[_reg + 8 * (_SrcReg)->Index]);

#define SET_RESULT(_reg, _src_reg)              \
    mov(_reg, ptr[rdx + Scope::OffsetRegs()]);  \
    add(_reg, runtime::Array::OffsetTable());   \
    mov(ptr[_reg + 8 * inst->Index], _src_reg);

CompileTask::Func BaselineCompileTask::Compile()
{
    IR->UpdateIndex();

    std::vector<Label> labels(IR->Blocks.size());
    Label returnPoint, throwPoint;

    push(rbp);
    mov(rbp, rsp);

    push(rbx);
    push(r10);
    push(r15);

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
                push(rcx);
                push(rdx);
                push(r8);
                push(r9);
                push(r15);

                LOAD_REG(rax, inst->As<ir::GetItem>()->_Obj);
                LOAD_REG(rbx, inst->As<ir::GetItem>()->_Key);

                // &error
                push(r9);

                // &retVal
                mov(r9, ptr[rdx + Scope::OffsetRegs()]);
                add(r9, runtime::Array::OffsetTable());
                lea(r9, ptr[r9 + 8 * inst->Index]);
                push(r9);

                // key
                mov(r8, rbx);
                push(r8);

                // object
                mov(rdx, rax);
                push(rdx);

                // &allocator
                push(rcx);

                call(runtime::semantic::ObjectGet);
                add(rsp, 40);

                pop(r15);
                pop(r9);
                pop(r8);
                pop(rdx);
                pop(rcx);

                test(rax, rax);
                jz(throwPoint);

                break;
            }
            case SET_ITEM:
            {
                push(rcx);
                push(rdx);
                push(r8);
                push(r9);
                push(r15);

                LOAD_REG(rax, inst->As<ir::SetItem>()->_Obj);
                LOAD_REG(rbx, inst->As<ir::SetItem>()->_Key);
                LOAD_REG(r10, inst->As<ir::SetItem>()->_Value);

                // &error
                push(r9);

                // value
                mov(r9, r10);
                push(r9);

                // key
                mov(r8, rbx);
                push(r8);

                // object
                mov(rdx, rax);
                push(rdx);

                // &allocator
                push(rcx);

                call(runtime::semantic::ObjectSet);
                add(rsp, 40);

                pop(r15);
                pop(r9);
                pop(r8);
                pop(rdx);
                pop(rcx);

                test(rax, rax);
                jz(throwPoint);

                break;
            }
            case DEL_ITEM:
            {
                push(rcx);
                push(rdx);
                push(r8);
                push(r9);
                push(r15);

                LOAD_REG(rax, inst->As<ir::DelItem>()->_Obj);
                LOAD_REG(rbx, inst->As<ir::DelItem>()->_Key);

                // &error
                push(r9);

                // key
                mov(r8, rbx);
                push(r8);

                // object
                mov(rdx, rax);
                push(rdx);

                // &allocator
                push(rcx);

                call(runtime::semantic::ObjectDelete);
                add(rsp, 32);

                pop(r15);
                pop(r9);
                pop(r8);
                pop(rdx);
                pop(rcx);

                test(rax, rax);
                jz(throwPoint);

                break;
            }
            case NEW:
            {
                push(rcx);
                push(rdx);
                push(r8);
                push(r9);
                push(r15);

                LOAD_REG(rax, inst->As<ir::New>()->_Callee);
                LOAD_REG(rbx, inst->As<ir::New>()->_Args);

                // &error
                push(r9);

                // &retVal
                mov(r9, ptr[rdx + Scope::OffsetRegs()]);
                add(r9, runtime::Array::OffsetTable());
                lea(r9, ptr[r9 + 8 * inst->Index]);
                push(r9);

                // *arguments
                mov(r8, rbx);
                and(r8, r15);
                push(r8);

                // constructor
                mov(rdx, rax);
                push(rdx);

                // &allocator
                push(rcx);

                call(runtime::semantic::NewObject);
                add(rsp, 40);

                pop(r15);
                pop(r9);
                pop(r8);
                pop(rdx);
                pop(rcx);

                test(rax, rax);
                jz(throwPoint);

                break;
            }
            case CALL:
            {
                push(rcx);
                push(rdx);
                push(r8);
                push(r9);
                push(r15);

                LOAD_REG(rax, inst->As<ir::Call>()->_Callee);
                LOAD_REG(r10, inst->As<ir::Call>()->_ThisArg);
                LOAD_REG(rbx, inst->As<ir::Call>()->_Args);

                // &error
                push(r9);

                // &retVal
                mov(r9, ptr[rdx + Scope::OffsetRegs()]);
                add(r9, runtime::Array::OffsetTable());
                lea(r9, ptr[r9 + 8 * inst->Index]);
                push(r9);

                // *arguments
                mov(r9, rbx);
                and(r9, r15);
                push(r9);

                // thisArg
                mov(r8, r10);
                and(r8, r15);
                push(r8);

                // callee
                mov(rdx, rax);
                push(rdx);

                // &allocator
                push(rcx);

                call(runtime::semantic::Call);
                add(rsp, 48);

                pop(r15);
                pop(r9);
                pop(r8);
                pop(rdx);
                pop(rcx);

                test(rax, rax);
                jz(throwPoint);

                break;
            }
            case GET_GLOBAL:
            {
                push(rcx);
                push(rdx);
                push(r8);
                push(r9);

                // &error
                push(r9);

                // &retVal
                mov(r8, ptr[rdx + Scope::OffsetRegs()]);
                add(r8, runtime::Array::OffsetTable());
                lea(r8, ptr[r8 + 8 * inst->Index]);
                push(r8);

                // name
                mov(rdx, reinterpret_cast<uintptr_t>(inst->As<ir::GetGlobal>()->Name));
                push(rdx);

                // &allocator
                push(rcx);

                call(runtime::semantic::GetGlobal);
                add(rsp, 32);

                pop(r9);
                pop(r8);
                pop(rdx);
                pop(rcx);

                test(rax, rax);
                jz(throwPoint);

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
            case REGEX:
            {
                hydra_trap("Not supporting regex");
                break;
            }
            case OBJECT:
            {
                push(rcx);
                push(rdx);
                push(r8);
                push(r9);

                // &error
                push(r9);

                // &retVal
                mov(r9, ptr[rdx + Scope::OffsetRegs()]);
                add(r9, runtime::Array::OffsetTable());
                lea(r9, ptr[r8 + 8 * inst->Index]);
                push(r9);

                // inst
                mov(r8, reinterpret_cast<uintptr_t>(inst.get()));
                push(r8);

                // scope
                push(rdx);

                // &allocator
                push(rcx);

                call(runtime::semantic::NewObjectWithInst);
                add(rsp, 32);

                pop(r9);
                pop(r8);
                pop(rdx);
                pop(rcx);

                test(rax, rax);
                jz(throwPoint);

                break;
            }
            case ARRAY:
            {
                push(rcx);
                push(rdx);
                push(r8);
                push(r9);

                // &error
                push(r9);

                // &retVal
                mov(r9, ptr[rdx + Scope::OffsetRegs()]);
                add(r9, runtime::Array::OffsetTable());
                lea(r9, ptr[r8 + 8 * inst->Index]);
                push(r9);

                // inst
                mov(r8, reinterpret_cast<uintptr_t>(inst.get()));
                push(r8);

                // scope
                push(rdx);

                // &allocator
                push(rcx);

                call(runtime::semantic::NewArrayWithInst);
                add(rsp, 40);

                pop(r9);
                pop(r8);
                pop(rdx);
                pop(rcx);

                test(rax, rax);
                jz(throwPoint);

                break;
            }
            case FUNC:
            {
                auto funcInst = inst->As<ir::Func>();
                if (funcInst->FuncId < IR->Module->Functions.size())
                {
                    funcInst->FuncPtr = IR->Module->Functions[funcInst->FuncId].get();
                }

                push(rcx);
                push(rdx);
                push(r8);
                push(r9);

                // &error
                push(r9);

                // &retVal
                mov(r9, ptr[rdx + Scope::OffsetRegs()]);
                add(r9, runtime::Array::OffsetTable());
                lea(r9, ptr[r8 + 8 * inst->Index]);
                push(r9);

                // inst
                mov(r8, reinterpret_cast<uintptr_t>(inst.get()));
                push(r8);

                // scope
                push(rdx);

                // &allocator
                push(rcx);

                call(runtime::semantic::NewFuncWithInst);
                add(rsp, 40);

                pop(r9);
                pop(r8);
                pop(rdx);
                pop(rcx);

                test(rax, rax);
                jz(throwPoint);

                break;
            }
            case ARROW:
            {
                auto funcInst = inst->As<ir::Func>();
                if (funcInst->FuncId < IR->Module->Functions.size())
                {
                    funcInst->FuncPtr = IR->Module->Functions[funcInst->FuncId].get();
                }

                push(rcx);
                push(rdx);
                push(r8);
                push(r9);

                // &error
                push(r9);

                // &retVal
                mov(r9, ptr[rdx + Scope::OffsetRegs()]);
                add(r9, runtime::Array::OffsetTable());
                lea(r9, ptr[r8 + 8 * inst->Index]);
                push(r9);

                // inst
                mov(r8, reinterpret_cast<uintptr_t>(inst.get()));
                push(r8);

                // scope
                push(rdx);

                // &allocator
                push(rcx);

                call(runtime::semantic::NewArrowWithInst);
                add(rsp, 40);

                pop(r9);
                pop(r8);
                pop(rdx);
                pop(rcx);

                test(rax, rax);
                jz(throwPoint);

                break;
            }
            }
        }
    }

    L(returnPoint);
    mov(ptr[r8], rax);
    mov(rax, 1);
    pop(r15);
    pop(r10);
    pop(rbx);
    pop(rbp);
    ret();

    L(throwPoint);
    mov(rax, 0);
    pop(r15);
    pop(r10);
    pop(rbx);
    pop(rbp);
    ret();

    return GetCode();
}

} // namespace vm
} // namespace hydra