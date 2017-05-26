#ifndef _IR_INST_H_
#define _IR_INST_H_

#include "Common/HydraCore.h"
#include "Runtime/String.h"

#include "IR.h"

#include <set>
#include <list>
#include <memory>
#include <type_traits>

#pragma push_macro("NULL")
#pragma push_macro("TRUE")
#pragma push_macro("FALSE")
#pragma push_macro("IN")
#pragma push_macro("THIS")

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

struct IRFunc;

enum INSTS
{
    RETURN = 0,         // <value>

    LOAD = 1,           // <addr>
    STORE = 2,          // <addr> <value>
    GET_ITEM = 3,       // <object> <key>
    SET_ITEM = 4,       // <object> <key> <value>
    DEL_ITEM = 5,       // <object> <key>
    NEW = 6,            // <callee> <args>
    CALL = 7,           // <callee> <this_arg> <args>

    JUMP = 8,           // <dst>
    BRANCH = 9,         // <condition> <consequence> <alternative>
    GET_GLOBAL = 10,    // <name>
    SET_GLOBAL = 11,    // <name> <value>

    UNDEFINED = 20,
    NULL = 21,
    TRUE = 22,
    FALSE = 23,
    NUMBER = 24,        // <value>
    STRING = 25,        // <stringPoolIndex>
    REGEX = 26,         // <regexLiteral>
    OBJECT = 27,        // <num> [<key> <value>]
    ARRAY = 28,         // <num> [<value>]
    FUNC = 29,          // <id>
    ARROW = 30,         // <id>

    ADD = 40,           // <a> <b>
    SUB = 41,           // <a> <b>
    MUL = 42,           // <a> <b>
    DIV = 43,           // <a> <b>
    MOD = 44,           // <a> <b>

    BAND = 45,          // <a> <b>
    BOR = 46,           // <a> <b>
    BXOR = 47,          // <a> <b>
    BNOT = 48,          // <a>

    LNOT = 49,          // <a>

    SLL = 50,           // <a> <b>
    SRL = 51,           // <a> <b>
    SRR = 52,           // <a> <b>

    EQ = 53,            // <a> <b>
    EQQ = 54,           // <a> <b>
    NE = 55,            // <a> <b>
    NEE = 56,           // <a> <b>
    GT = 57,            // <a> <b>
    GE = 58,            // <a> <b>
    LT = 59,            // <a> <b>
    LE = 60,            // <a> <b>

    IN = 61,            // <a> <b>
    INSTANCEOF = 62,    // <a> <b>
    TYPEOF = 63,        // <a>

    PUSH_SCOPE = 70,    // <size>
    POP_SCOPE = 71,     //
    ALLOCA = 72,
    ARG = 73,
    CAPTURE = 74,

    THIS = 80,
    ARGUMENTS = 81,
    MOVE = 82,          // <other>

    PHI = 90,           // <num> [<branch> <value>]

    DEBUGGER = 100,
};

namespace ir
{

#define DECL_INST(TYPE)                                 \
    virtual size_t GetType() const override final       \
    { return TYPE; }                                    \

struct Return : public IRInst
{
    DECL_INST(RETURN)
    Ref _Value;
};

struct Load : public IRInst
{
    DECL_INST(LOAD)
    Ref _Addr;
};

struct Store : public IRInst
{
    DECL_INST(STORE)
    Ref _Addr;
    Ref _Value;
};

struct GetItem : public IRInst
{
    DECL_INST(GET_ITEM)
    Ref _Obj;
    Ref _Key;
};

struct SetItem : public IRInst
{
    DECL_INST(SET_ITEM)
    Ref _Obj;
    Ref _Key;
    Ref _Value;
};

struct DelItem : public IRInst
{
    DECL_INST(DEL_ITEM)
    Ref _Obj;
    Ref _Key;
};

struct New : public IRInst
{
    DECL_INST(NEW)
    Ref _Callee;
    Ref _Args;
};

struct Call : public IRInst
{
    DECL_INST(CALL)
    Ref _Callee;
    Ref _ThisArg;
    Ref _Args;
};

struct GetGlobal : public IRInst
{
    DECL_INST(GET_GLOBAL)
    runtime::String *Name;
};

struct SetGlobal : public IRInst
{
    DECL_INST(GET_GLOBAL)
    runtime::String *Name;
    Ref _Value;
};

struct Undefined : public IRInst
{
    DECL_INST(UNDEFINED)
};

struct Null : public IRInst
{
    DECL_INST(NULL)
};

struct True : public IRInst
{
    DECL_INST(TRUE)
};

struct False : public IRInst
{
    DECL_INST(FALSE)
};

struct Number : public IRInst
{
    DECL_INST(NUMBER)
    double Value;
};

struct String : public IRInst
{
    DECL_INST(STRING)
    runtime::String *Value;
};

struct Object : public IRInst
{
    DECL_INST(OBJECT)
    std::list<std::pair<Ref, Ref> > Initialization;
};

struct Array : public IRInst
{
    DECL_INST(ARRAY)
    std::list<Ref> Initialization;
};

struct Func : public IRInst
{
    DECL_INST(FUNC)
    union
    {
        IRFunc *FuncPtr;
        size_t FuncId;
    };
    std::list<Ref> Captured;
};

struct Arrow : public IRInst
{
    DECL_INST(ARROW)
    union
    {
        IRFunc *FuncPtr;
        size_t FuncId;
    };
    std::list<Ref> Captured;
};

struct Binary : public IRInst
{
    Ref _A;
    Ref _B;
};

struct Add : public Binary
{
    DECL_INST(ADD)
};

struct Sub : public Binary
{
    DECL_INST(SUB)
};

struct Mul : public Binary
{
    DECL_INST(MUL)
};

struct Div : public Binary
{
    DECL_INST(DIV)
};

struct Mod : public Binary
{
    DECL_INST(MOD)
};

struct Band : public Binary
{
    DECL_INST(BAND)
};

struct Bor : public Binary
{
    DECL_INST(BOR)
};

struct Bxor : public Binary
{
    DECL_INST(BXOR)
};

struct Sll : public Binary
{
    DECL_INST(SLL)
};

struct Srl : public Binary
{
    DECL_INST(SRL)
};

struct Srr : public Binary
{
    DECL_INST(SRR)
};

struct Eq : public Binary
{
    DECL_INST(EQ)
};

struct Eqq : public Binary
{
    DECL_INST(EQQ)
};

struct Ne : public Binary
{
    DECL_INST(NE)
};

struct Nee : public Binary
{
    DECL_INST(NEE)
};

struct Gt : public Binary
{
    DECL_INST(GT)
};

struct Ge : public Binary
{
    DECL_INST(GE)
};

struct Lt : public Binary
{
    DECL_INST(LT)
};

struct Le : public Binary
{
    DECL_INST(LE)
};

struct In : public Binary
{
    DECL_INST(IN)
};

struct InstanceOf : public Binary
{
    DECL_INST(INSTANCEOF)
};

struct Unary : public IRInst
{
    Ref _A;
};

struct Bnot : public Unary
{
    DECL_INST(BNOT)
};

struct Lnot : public Unary
{
    DECL_INST(LNOT)
};

struct TypeOf : public Unary
{
    DECL_INST(TYPEOF)
};

struct PushScope : public IRInst
{
    DECL_INST(PUSH_SCOPE)
    size_t Size;
    std::list<Ref> Captured;
};

struct PopScope : public IRInst
{
    DECL_INST(POP_SCOPE)
    size_t Count;
};

struct Alloca : public IRInst
{
    DECL_INST(ALLOCA)
};

struct Arg : public IRInst
{
    DECL_INST(ARG)
    size_t Index;
};

struct Capture : public IRInst
{
    DECL_INST(CAPTURE)
    size_t Index;
};

struct This : public IRInst
{
    DECL_INST(THIS)
};

struct Arguments : public IRInst
{
    DECL_INST(ARGUMENTS)
};

struct Move : public IRInst
{
    DECL_INST(MOVE)
    Ref _Other;
};

struct Phi : public IRInst
{
    DECL_INST(PHI)
    std::list<std::pair<IRBlock::Ref, Ref>> Branches;
};

struct Debugger : public IRInst
{
    DECL_INST(DEBUGGER)
};

} // namespace ir

} // namespace vm
} // namespace hydra

#pragma pop_macro("THIS")
#pragma pop_macro("IN")
#pragma pop_macro("FALSE")
#pragma pop_macro("TRUE")
#pragma pop_macro("NULL")

#endif // _IR_INST_H_