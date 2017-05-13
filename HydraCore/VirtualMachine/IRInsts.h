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

struct Return : public IRInst
{
    Ref _Value;
};

struct Load : public IRInst
{
    Ref _Addr;
};

struct Store : public IRInst
{
    Ref _Addr;
    Ref _Value;
};

struct GetItem : public IRInst
{
    Ref _Obj;
    Ref _Key;
};

struct SetItem : public IRInst
{
    Ref _Obj;
    Ref _Key;
    Ref _Value;
};

struct DelItem : public IRInst
{
    Ref _Obj;
    Ref _Key;
};

struct New : public IRInst
{
    Ref _Callee;
    Ref _Args;
};

struct Call : public IRInst
{
    Ref _Callee;
    Ref _ThisArg;
    Ref _Args;
};

struct GetGlobal : public IRInst
{
    runtime::String *Name;
};

struct Undefined : public IRInst
{ };

struct Null : public IRInst
{ };

struct True : public IRInst
{ };

struct False : public IRInst
{ };

struct Number : public IRInst
{
    double Value;
};

struct String : public IRInst
{
    runtime::String *Value;
};

struct Object : public IRInst
{
    std::list<std::pair<Ref, Ref> > Initialization;
};

struct Array : public IRInst
{
    std::list<Ref> Initialization;
};

struct Func : public IRInst
{
    union
    {
        IRFunc *FuncPtr;
        size_t FuncId;
    };
    std::list<Ref> Captured;
};

struct Arrow : public IRInst
{
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
{ };

struct Sub : public Binary
{ };

struct Mul : public Binary
{ };

struct Div : public Binary
{ };

struct Mod : public Binary
{ };

struct Band : public Binary
{ };

struct Bor : public Binary
{ };

struct Bxor : public Binary
{ };

struct Sll : public Binary
{ };

struct Srl : public Binary
{ };

struct Srr : public Binary
{ };

struct Eq : public Binary
{ };

struct Eqq : public Binary
{ };

struct Ne : public Binary
{ };

struct Nee : public Binary
{ };

struct Gt : public Binary
{ };

struct Ge : public Binary
{ };

struct Lt : public Binary
{ };

struct Le : public Binary
{ };

struct In : public Binary
{ };

struct InstanceOf : public Binary
{ };

struct Unary : public IRInst
{
    Ref _A;
};

struct Bnot : public Unary
{ };

struct Lnot : public Unary
{ };

struct TypeOf : public Unary
{ };

struct PushScope : public IRInst
{
    size_t Size;
    std::list<Ref> Captured;
};

struct PopScope : public IRInst
{
    size_t Count;
};

struct Alloca : public IRInst
{ };

struct Arg : public IRInst
{
    size_t Index;
};

struct Capture : public IRInst
{
    size_t Index;
};

struct This : public IRInst
{ };

struct Arguments : public IRInst
{ };

struct Move : public IRInst
{
    Ref _Other;
};

struct Phi : public IRInst
{
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