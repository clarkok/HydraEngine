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

#define DUMP_START(name)                                \
    virtual void Dump(std::ostream &os) override final  \
    {                                                   \
        os << "\t" << InstIndex << "\t" << name << " "

#define DUMP_END()                                      \
        << std::endl;                                   \
    }

#define DUMP(name, seq)         DUMP_START(name) seq DUMP_END()

#define _()             << ", "

#define DUMP_REF(ref)   << "$" << ref->InstIndex
#define DUMP_STR(str)   << "\"" << str->ToString() << "\""
#define DUMP_NUM(num)   << num
#define DUMP_BLK(blk)   << "blk_" << blk->Index

struct Return : public IRInst
{
    DECL_INST(RETURN)
    Ref _Value;

    DUMP("ret", 
        DUMP_REF(_Value)
    )
};

struct Load : public IRInst
{
    DECL_INST(LOAD)
    Ref _Addr;

    DUMP("load",
        DUMP_REF(_Addr)
    )
};

struct Store : public IRInst
{
    DECL_INST(STORE)
    Ref _Addr;
    Ref _Value;

    DUMP("store",
        DUMP_REF(_Addr) _()
        DUMP_REF(_Value)
    )
};

struct GetItem : public IRInst
{
    DECL_INST(GET_ITEM)
    Ref _Obj;
    Ref _Key;

    DUMP("get_item",
        DUMP_REF(_Obj) _()
        DUMP_REF(_Key)
    )
};

struct SetItem : public IRInst
{
    DECL_INST(SET_ITEM)
    Ref _Obj;
    Ref _Key;
    Ref _Value;

    DUMP("set_item",
        DUMP_REF(_Obj) _()
        DUMP_REF(_Key) _()
        DUMP_REF(_Value)
    )
};

struct DelItem : public IRInst
{
    DECL_INST(DEL_ITEM)
    Ref _Obj;
    Ref _Key;

    DUMP("del_item",
        DUMP_REF(_Obj) _()
        DUMP_REF(_Key)
    )
};

struct New : public IRInst
{
    DECL_INST(NEW)
    Ref _Callee;
    Ref _Args;

    DUMP("new",
        DUMP_REF(_Callee) _()
        DUMP_REF(_Args)
    )
};

struct Call : public IRInst
{
    DECL_INST(CALL)
    Ref _Callee;
    Ref _ThisArg;
    Ref _Args;

    DUMP("call",
        DUMP_REF(_Callee) _()
        DUMP_REF(_ThisArg) _()
        DUMP_REF(_Args)
    )
};

struct GetGlobal : public IRInst
{
    DECL_INST(GET_GLOBAL)
    runtime::String *Name;

    DUMP("get_global",
        DUMP_STR(Name)
    )
};

struct SetGlobal : public IRInst
{
    DECL_INST(GET_GLOBAL)
    runtime::String *Name;
    Ref _Value;

    DUMP("set_global",
        DUMP_STR(Name) _()
        DUMP_REF(_Value)
    )
};

struct Undefined : public IRInst
{
    DECL_INST(UNDEFINED)

    DUMP("undefined", )
};

struct Null : public IRInst
{
    DECL_INST(NULL)

    DUMP("null", )
};

struct True : public IRInst
{
    DECL_INST(TRUE)

    DUMP("true", )
};

struct False : public IRInst
{
    DECL_INST(FALSE)

    DUMP("false", )
};

struct Number : public IRInst
{
    DECL_INST(NUMBER)
    double Value;

    DUMP("number",
        DUMP_NUM(Value)
    )
};

struct String : public IRInst
{
    DECL_INST(STRING)
    runtime::String *Value;

    DUMP("string",
        DUMP_STR(Value)
    )
};

struct Object : public IRInst
{
    DECL_INST(OBJECT)
    std::list<std::pair<Ref, Ref> > Initialization;

    DUMP_START("object") << " [";
        for (auto &pair : Initialization)
        {
            os << " (" DUMP_REF(pair.first) _() DUMP_REF(pair.second) << ")";
        }
        os << " ]"
    DUMP_END()
};

struct Array : public IRInst
{
    DECL_INST(ARRAY)
    std::list<Ref> Initialization;

    DUMP_START("array") << " [";
        for (auto &ref : Initialization)
        {
            os << " " DUMP_REF(ref);
        }
        os << " ]"
    DUMP_END()
};

struct Func : public IRInst
{
    DECL_INST(FUNC)
    IRFunc *FuncPtr;
    size_t FuncId;
    std::list<Ref> Captured;

    DUMP_START("func") DUMP_NUM(FuncId) << " [";
        for (auto &ref : Captured)
        {
            os << " " DUMP_REF(ref);
        }
        os << " ]"
    DUMP_END()
};

struct Arrow : public IRInst
{
    DECL_INST(ARROW)
    IRFunc *FuncPtr;
    size_t FuncId;
    std::list<Ref> Captured;

    DUMP_START("arrow") DUMP_NUM(FuncId) << " [";
        for (auto &ref : Captured)
        {
            os << " " DUMP_REF(ref);
        }
        os << " ]"
    DUMP_END()
};

struct Binary : public IRInst
{
    Ref _A;
    Ref _B;
};

#define DUMP_BINARY(name)       \
    DUMP(name, DUMP_REF(_A) _() DUMP_REF(_B))

struct Add : public Binary
{
    DECL_INST(ADD)
    DUMP_BINARY("add")
};

struct Sub : public Binary
{
    DECL_INST(SUB)
    DUMP_BINARY("sub")
};

struct Mul : public Binary
{
    DECL_INST(MUL)
    DUMP_BINARY("mul")
};

struct Div : public Binary
{
    DECL_INST(DIV)
    DUMP_BINARY("div")
};

struct Mod : public Binary
{
    DECL_INST(MOD)
    DUMP_BINARY("mod")
};

struct Band : public Binary
{
    DECL_INST(BAND)
    DUMP_BINARY("band")
};

struct Bor : public Binary
{
    DECL_INST(BOR)
    DUMP_BINARY("bor")
};

struct Bxor : public Binary
{
    DECL_INST(BXOR)
    DUMP_BINARY("bxor")
};

struct Sll : public Binary
{
    DECL_INST(SLL)
    DUMP_BINARY("sll")
};

struct Srl : public Binary
{
    DECL_INST(SRL)
    DUMP_BINARY("srl")
};

struct Srr : public Binary
{
    DECL_INST(SRR)
    DUMP_BINARY("srr")
};

struct Eq : public Binary
{
    DECL_INST(EQ)
    DUMP_BINARY("eq")
};

struct Eqq : public Binary
{
    DECL_INST(EQQ)
    DUMP_BINARY("eqq")
};

struct Ne : public Binary
{
    DECL_INST(NE)
    DUMP_BINARY("ne")
};

struct Nee : public Binary
{
    DECL_INST(NEE)
    DUMP_BINARY("nee")
};

struct Gt : public Binary
{
    DECL_INST(GT)
    DUMP_BINARY("gt")
};

struct Ge : public Binary
{
    DECL_INST(GE)
    DUMP_BINARY("ge")
};

struct Lt : public Binary
{
    DECL_INST(LT)
    DUMP_BINARY("lt")
};

struct Le : public Binary
{
    DECL_INST(LE)
    DUMP_BINARY("le")
};

struct In : public Binary
{
    DECL_INST(IN)
    DUMP_BINARY("in")
};

struct InstanceOf : public Binary
{
    DECL_INST(INSTANCEOF)
    DUMP_BINARY("instanceof")
};

#undef DUMP_BINARY

struct Unary : public IRInst
{
    Ref _A;
};

#define DUMP_UNARY(name)    \
    DUMP(name, DUMP_REF(_A))

struct Bnot : public Unary
{
    DECL_INST(BNOT)
    DUMP_UNARY("bnot")
};

struct Lnot : public Unary
{
    DECL_INST(LNOT)
    DUMP_UNARY("lnot")
};

struct TypeOf : public Unary
{
    DECL_INST(TYPEOF)
    DUMP_UNARY("typeof")
};

#undef DUMP_UNARY

struct PushScope : public IRInst
{
    DECL_INST(PUSH_SCOPE)
    size_t Size;
    std::list<Ref> Captured;

    DUMP_START("push_scope") DUMP_NUM(Size) << " [";
        for (auto &ref : Captured)
        {
            os << " " DUMP_REF(ref);
        }
        os << " ]"
    DUMP_END()

    Ref UpperScope;
    std::set<Ref> InnerScopes;
    IRBlock::Ref OwnerBlock;
};

struct PopScope : public IRInst
{
    DECL_INST(POP_SCOPE)

    DUMP("pop_scope")

    Ref Scope;
};

struct Alloca : public IRInst
{
    DECL_INST(ALLOCA)

    DUMP("alloca", )
};

struct Arg : public IRInst
{
    DECL_INST(ARG)
    size_t Index;

    DUMP("arg", DUMP_NUM(Index))
};

struct Capture : public IRInst
{
    DECL_INST(CAPTURE)
    size_t Index;

    DUMP("capture", DUMP_NUM(Index))
};

struct This : public IRInst
{
    DECL_INST(THIS)

    DUMP("this", )
};

struct Arguments : public IRInst
{
    DECL_INST(ARGUMENTS)

    DUMP("arguments", )
};

struct Move : public IRInst
{
    DECL_INST(MOVE)
    Ref _Other;

    DUMP("move", DUMP_REF(_Other))
};

struct Phi : public IRInst
{
    DECL_INST(PHI)
    std::list<std::pair<IRBlock::Ref, Ref>> Branches;

    DUMP_START("phi") << " [";
        for (auto &pair : Branches)
        {
            os << " (" DUMP_BLK(pair.first) _() DUMP_REF(pair.second) << ")";
        }
        os << " ]"
    DUMP_END()
};

struct Debugger : public IRInst
{
    DECL_INST(DEBUGGER)

    DUMP("debugger", )
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