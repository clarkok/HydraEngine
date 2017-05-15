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

    UNDEFINED = 11,
    NULL = 12,
    TRUE = 13,
    FALSE = 14,
    NUMBER = 15,        // <value>
    STRING = 16,        // <stringPoolIndex>
    REGEX = 17,         // <regexLiteral>
    OBJECT = 18,        // <num> [<key> <value>]
    ARRAY = 19,         // <num> [<value>]
    FUNC = 20,          // <id>
    ARROW = 21,         // <id>

    ADD = 22,           // <a> <b>
    SUB = 23,           // <a> <b>
    MUL = 24,           // <a> <b>
    DIV = 25,           // <a> <b>
    MOD = 26,           // <a> <b>

    BAND = 27,          // <a> <b>
    BOR = 28,           // <a> <b>
    BXOR = 29,          // <a> <b>
    BNOT = 30,          // <a>

    LNOT = 31,          // <a>

    SLL = 32,           // <a> <b>
    SRL = 33,           // <a> <b>
    SRR = 34,           // <a> <b>

    EQ = 35,            // <a> <b>
    EQQ = 36,           // <a> <b>
    NE = 37,            // <a> <b>
    NEE = 38,           // <a> <b>
    GT = 39,            // <a> <b>
    GE = 40,            // <a> <b>
    LT = 41,            // <a> <b>
    LE = 42,            // <a> <b>

    IN = 43,            // <a> <b>
    INSTANCEOF = 44,    // <a> <b>
    TYPEOF = 45,        // <a>

    PUSH_SCOPE = 46,    // <size>
    POP_SCOPE = 47,     //
    ALLOCA = 48,
    ARG = 49,
    CAPTURE = 50,

    THIS = 51,
    ARGUMENTS = 52,
    MOVE = 53,          // <other>

    PHI = 54,           // <num> [<branch> <value>]
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

} // namespace ir

} // namespace vm
} // namespace hydra

#pragma pop_macro("THIS")
#pragma pop_macro("IN")
#pragma pop_macro("FALSE")
#pragma pop_macro("TRUE")
#pragma pop_macro("NULL")

#endif // _IR_INST_H_