#include "ByteCode.h"

#include "IRInsts.h"

#include "Compile.h"

#include <map>

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

ByteCode::ByteCode(std::string filename)
    : FileMapping(filename)
{
    if (!ParseHeader())
    {
        hydra_trap("Invalid ByteCode");
    }
}

bool ByteCode::ParseHeader()
{
    SectionReader reader(this, 0);

    u32 magic;

    auto result = reader.Uint(magic);
    hydra_assert(result, "Error on reading magic word");
    hydra_assert(magic == MAGIC_WORD, "Magic word mismatch");

    u32 sectionCount;
    result = reader.Uint(sectionCount);
    hydra_assert(result, "Error on reading section count");

    StringPoolOffset = 0;
    while (sectionCount--)
    {
        u32 type;
        u32 offset;

        result = reader.Uint(type);
        hydra_assert(result, "Error on reading section type");

        result = reader.Uint(offset);
        hydra_assert(result, "Error on reading section offset");

        if (type == ByteCodeSectionType::STRING_POOL)
        {
            hydra_assert(StringPoolOffset == 0, "Two string pools found in one bytecode");
            StringPoolOffset = offset;
        }
        Sections.emplace_back(std::pair<u32, u32>{ type, offset });
    }

    return true;
}

std::unique_ptr<IRModule> ByteCode::Load(gc::ThreadAllocator &allocator)
{
    runtime::JSArray *strings = runtime::JSArray::New(allocator);
    std::unique_ptr<IRModule> ret(new IRModule(strings));
    std::map<u32, runtime::String *> stringMap;

    LoadStringPool(allocator, strings, stringMap);

    for (auto section : Sections)
    {
        if (section.first == FUNCTION)
        {
            ret->Functions.emplace_back(LoadFunction(section.second, stringMap));
            ret->Functions.back()->Module = ret.get();
        }
    }

    return ret;
}

void ByteCode::LoadStringPool(
    gc::ThreadAllocator &allocator,
    runtime::JSArray *strings,
    std::map<u32, runtime::String *> &stringMap)
{
    hydra_assert(StringPoolOffset != 0,
        "No string pool found");

    SectionReader reader(this, StringPoolOffset);

    u32 length;
    std::pair<const char_t *, const char_t *> range;

    u32 current;
    while (reader.String(length, current, range))
    {
        runtime::String *str = runtime::String::New(allocator, range.first, range.second);
        stringMap[current] = str;
        strings->Set(allocator, strings->GetLength(), runtime::JSValue::FromString(str));
    }
}

std::unique_ptr<IRFunc> ByteCode::LoadFunction(
    size_t section,
    std::map<u32, runtime::String *> &stringMap)
{
    std::vector<IRInst *> insts;
    std::vector<IRBlock *> blocks;
    bool result;
    std::unique_ptr<IRFunc> ret;

#pragma region FIRST_SCAN
    {
        SectionReader reader(this, section);
        u32 funcName;
        result = reader.Uint(funcName);
        hydra_assert(result, "Error on reading function name");

        runtime::String *name = stringMap[funcName];
        hydra_assert(name != nullptr, "Invalid function name");

        u32 length;
        result = reader.Uint(length);
        hydra_assert(result, "Error on reading function length");

        ret.reset(new IRFunc(nullptr, stringMap[funcName], length));

        u32 blockCount;
        result = reader.Uint(blockCount);
        hydra_assert(result, "Error on reading block count");

        while (blockCount--)
        {
            ret->Blocks.emplace_back(new IRBlock());
            auto block = ret->Blocks.back().get();

            blocks.push_back(block);

            u32 instCount;
            result = reader.Uint(instCount);
            hydra_assert(result, "Error on reading inst count");

            while (instCount--)
            {
                u32 type;
                u32 placeHolder;
                result = reader.Uint(type);
                hydra_assert(result, "Error on reading inst type");

#define FIRST_INST(_case, _class, _n)                   \
    case _case:                                         \
        inst.reset(new ir::_class ());                  \
        hydra_rep(_n, { reader.Uint(placeHolder); });   \
        break

                std::unique_ptr<IRInst> inst;
                switch (type)
                {
                    FIRST_INST(RETURN, Return, 1);
                    FIRST_INST(LOAD, Load, 1);
                    FIRST_INST(STORE, Store, 2);
                    FIRST_INST(GET_ITEM, GetItem, 2);
                    FIRST_INST(SET_ITEM, SetItem, 3);
                    FIRST_INST(DEL_ITEM, DelItem, 2);
                    FIRST_INST(NEW, New, 2);
                    FIRST_INST(CALL, Call, 3);
                    FIRST_INST(GET_GLOBAL, GetGlobal, 1);
                    FIRST_INST(SET_GLOBAL, SetGlobal, 2);
                    FIRST_INST(UNDEFINED, Undefined, 0);
                    FIRST_INST(NULL, Null, 0);
                    FIRST_INST(TRUE, True, 0);
                    FIRST_INST(FALSE, False, 0);
                    FIRST_INST(NUMBER, Number, 2);  // a double equal to 2 uint
                    FIRST_INST(STRING, String, 1);
                    FIRST_INST(ADD, Add, 2);
                    FIRST_INST(SUB, Sub, 2);
                    FIRST_INST(MUL, Mul, 2);
                    FIRST_INST(DIV, Div, 2);
                    FIRST_INST(MOD, Mod, 2);
                    FIRST_INST(BAND, Band, 2);
                    FIRST_INST(BOR, Bor, 2);
                    FIRST_INST(BXOR, Bxor, 2);
                    FIRST_INST(SLL, Sll, 2);
                    FIRST_INST(SRL, Srl, 2);
                    FIRST_INST(SRR, Srr, 2);
                    FIRST_INST(EQ, Eq, 2);
                    FIRST_INST(EQQ, Eqq, 2);
                    FIRST_INST(NE, Ne, 2);
                    FIRST_INST(NEE, Nee, 2);
                    FIRST_INST(GT, Gt, 2);
                    FIRST_INST(GE, Ge, 2);
                    FIRST_INST(LT, Lt, 2);
                    FIRST_INST(LE, Le, 2);
                    FIRST_INST(IN, In, 2);
                    FIRST_INST(INSTANCEOF, InstanceOf, 2);
                    FIRST_INST(BNOT, Bnot, 1);
                    FIRST_INST(LNOT, Lnot, 1);
                    FIRST_INST(TYPEOF, TypeOf, 1);
                    FIRST_INST(POP_SCOPE, PopScope, 1);
                    FIRST_INST(ALLOCA, Alloca, 0);
                    FIRST_INST(ARG, Arg, 1);
                    FIRST_INST(CAPTURE, Capture, 1);
                    FIRST_INST(THIS, This, 0);
                    FIRST_INST(ARGUMENTS, Arguments, 0);
                    FIRST_INST(MOVE, Move, 1);
                case OBJECT:
                    inst.reset(new ir::Object());
                    {
                        u32 length;
                        reader.Uint(length);
                        while (length--)
                        {
                            reader.Uint(placeHolder);
                            reader.Uint(placeHolder);
                        }
                    }
                    break;
                case ARRAY:
                    inst.reset(new ir::Array());
                    {
                        u32 length;
                        reader.Uint(length);
                        while (length--)
                        {
                            reader.Uint(placeHolder);
                        }
                    }
                    break;
                case FUNC:
                    inst.reset(new ir::Func());
                    {
                        reader.Uint(placeHolder);
                        u32 length;
                        reader.Uint(length);
                        while (length--)
                        {
                            reader.Uint(placeHolder);
                        }
                    }
                    break;
                case ARROW:
                    inst.reset(new ir::Arrow());
                    {
                        reader.Uint(placeHolder);
                        u32 length;
                        reader.Uint(length);
                        while (length--)
                        {
                            reader.Uint(placeHolder);
                        }
                    }
                    break;
                case PUSH_SCOPE:
                    inst.reset(new ir::PushScope());
                    {
                        reader.Uint(placeHolder);
                        u32 length;
                        reader.Uint(length);
                        while (length--)
                        {
                            reader.Uint(placeHolder);
                        }
                    }
                    break;
                case PHI:
                    inst.reset(new ir::Phi());
                    {
                        u32 length;
                        reader.Uint(length);
                        while (length--)
                        {
                            reader.Uint(placeHolder);
                            reader.Uint(placeHolder);
                        }
                    }
                    break;
                case DEBUGGER:
                    inst.reset(new ir::Debugger());
                    break;
                case JUMP:
                    reader.Uint(placeHolder);
                    break;
                case BRANCH:
                    reader.Uint(placeHolder);
                    reader.Uint(placeHolder);
                    reader.Uint(placeHolder);
                    break;
                default:
                    hydra_trap("Unknown inst type");
                }

#undef FIRST_INST

                insts.push_back(inst.get());
                if (inst.get())
                {
                    block->Insts.emplace_back(std::move(inst));
                }
            }
        }
    }
#pragma endregion FIRST_SCAN

#pragma region SECION_SCAN
    {
        SectionReader reader(this, section);

        u32 funcName;
        reader.Uint(funcName);

        u32 length;
        reader.Uint(length);

        u32 blockCount;
        reader.Uint(blockCount);

        for (auto &block : ret->Blocks)
        {
            u32 instCount;
            reader.Uint(instCount);

            for (auto &inst : block->Insts)
            {
                --instCount;
                u32 type;
                reader.Uint(type);
#define As(_type)                       dynamic_cast<ir::_type *>(inst.get())
#define LoadRegister(_type, _member)    \
    { reader.Uint(other); As(_type)->_member = insts[other]; }
#define LoadHydraString(_type, _member)      \
    { reader.Uint(other), As(_type)->_member = stringMap[other]; }
#define LoadDouble(_type, _member)      \
    { reader.Double(As(_type)->_member); }
#define LoadSizeT(_type, _member)       \
    { reader.Uint(other); As(_type)->_member = other; }

                u32 other;
                switch (type)
                {
                case RETURN:
                    LoadRegister(Return, _Value);
                    break;
                case LOAD:
                    LoadRegister(Load, _Addr);
                    break;
                case STORE:
                    LoadRegister(Store, _Addr);
                    LoadRegister(Store, _Value);
                    break;
                case GET_ITEM:
                    LoadRegister(GetItem, _Obj);
                    LoadRegister(GetItem, _Key);
                    break;
                case SET_ITEM:
                    LoadRegister(SetItem, _Obj);
                    LoadRegister(SetItem, _Key);
                    LoadRegister(SetItem, _Value);
                    break;
                case DEL_ITEM:
                    LoadRegister(DelItem, _Obj);
                    LoadRegister(DelItem, _Key);
                    break;
                case NEW:
                    LoadRegister(New, _Callee);
                    LoadRegister(New, _Args);
                    break;
                case CALL:
                    LoadRegister(Call, _Callee);
                    LoadRegister(Call, _ThisArg);
                    LoadRegister(Call, _Args);
                    break;
                case GET_GLOBAL:
                    LoadHydraString(GetGlobal, Name);
                    break;
                case SET_GLOBAL:
                    LoadHydraString(SetGlobal, Name);
                    LoadRegister(SetGlobal, _Value);
                    break;
                case UNDEFINED:
                case NULL:
                case TRUE:
                case FALSE:
                    break;
                case NUMBER:
                    LoadDouble(Number, Value);
                    break;
                case STRING:
                    LoadHydraString(String, Value);
                    break;
                case OBJECT:
                {
                    u32 length;
                    reader.Uint(length);
                    auto &initialization = As(Object)->Initialization;
                    while (length--)
                    {
                        u32 key;
                        u32 value;
                        reader.Uint(key);
                        reader.Uint(value);

                        initialization.emplace_back(std::pair<IRInst::Ref, IRInst::Ref>(
                            insts[key],
                            insts[value]));
                    }
                    break;
                }
                case ARRAY:
                {
                    u32 length;
                    reader.Uint(length);
                    auto &initialization = As(Array)->Initialization;
                    while (length--)
                    {
                        u32 value;
                        reader.Uint(value);

                        initialization.emplace_back(insts[value]);
                    }
                    break;
                }
                case FUNC:
                {
                    u32 funcId;
                    reader.Uint(funcId);
                    As(Func)->FuncId = funcId;

                    u32 length;
                    reader.Uint(length);
                    auto &captured = As(Func)->Captured;
                    while (length--)
                    {
                        reader.Uint(other);
                        captured.emplace_back(insts[other]);
                    }
                    break;
                }
                case ARROW:
                {
                    u32 funcId;
                    reader.Uint(funcId);
                    As(Arrow)->FuncId = funcId;

                    u32 length;
                    reader.Uint(length);
                    auto &captured = As(Arrow)->Captured;
                    while (length--)
                    {
                        reader.Uint(other);
                        captured.emplace_back(insts[other]);
                    }
                    break;
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
                    LoadRegister(Binary, _A);
                    LoadRegister(Binary, _B);
                    break;
                case BNOT:
                case LNOT:
                case TYPEOF:
                    LoadRegister(Unary, _A);
                    break;
                case PUSH_SCOPE:
                {
                    u32 size;
                    reader.Uint(size);
                    As(PushScope)->Size = size;

                    u32 length;
                    reader.Uint(length);
                    auto &captured = As(PushScope)->Captured;
                    while (length--)
                    {
                        reader.Uint(other);
                        captured.emplace_back(insts[other]);
                    }
                    break;
                }
                case POP_SCOPE:
                    LoadSizeT(PopScope, Count);
                    break;
                case ALLOCA:
                    break;
                case ARG:
                    LoadSizeT(Arg, Index);
                    break;
                case CAPTURE:
                    LoadSizeT(Capture, Index);
                case THIS:
                case ARGUMENTS:
                    break;
                case MOVE:
                    LoadRegister(Move, _Other);
                    break;
                case PHI:
                {
                    u32 length;
                    reader.Uint(length);

                    auto &branches = As(Phi)->Branches;
                    while (length--)
                    {
                        u32 precedence;
                        u32 value;

                        reader.Uint(precedence);
                        reader.Uint(value);
                        branches.emplace_back(std::pair<IRBlock::Ref, IRInst::Ref>(blocks[precedence], insts[value]));
                    }
                    break;
                }
                case DEBUGGER:
                    break;
                default:
                    hydra_trap("Internal");
                }
            }

            while (instCount--)
            {
                u32 type;
                reader.Uint(type);
                switch (type)
                {
                case JUMP:
                {
                    u32 dst;
                    reader.Uint(dst);
                    hydra_assert(block->Consequent.Ptr == nullptr,
                        "Invalid IR, multiple JUMP");
                    block->Consequent = blocks[dst];
                    break;
                }
                case BRANCH:
                {
                    u32 condition;
                    reader.Uint(condition);
                    hydra_assert(block->Condition.Ptr == nullptr,
                        "Invalid IR, multiple BRANCH");
                    block->Condition = insts[condition];

                    u32 consequent;
                    reader.Uint(consequent);
                    hydra_assert(block->Consequent.Ptr == nullptr,
                        "Invalid IR, multiple BRANCH");
                    block->Consequent = blocks[consequent];

                    u32 alternate;
                    reader.Uint(alternate);
                    hydra_assert(block->Alternate.Ptr == nullptr,
                        "Invalid IR, multiple BRANCH");
                    block->Alternate = blocks[alternate];

                    break;
                }
                default:
                    hydra_trap("Internal");
                }
            }
        }
    }
#pragma endregion SECOND_SCAN

    auto func = ret.get();

    ret->BaselineFuture = std::move(std::async(
        std::launch::deferred,
        [func]() -> CompiledFunction *
        {
            std::unique_ptr<CompileTask> task(new BaselineCompileTask(func));

            size_t registerCount;
            func->BaselineFunction = std::make_unique<CompiledFunction>(
                std::move(task),
                func->Length,
                func->GetVarCount());

            CompiledFunction *current = nullptr;
            func->Compiled.compare_exchange_strong(current, func->BaselineFunction.get());

            return func->BaselineFunction.get();
        }
    ));

    return ret;
}

} // namespace vm
} // namespace hydra

#pragma pop_macro("THIS")
#pragma pop_macro("IN")
#pragma pop_macro("FALSE")
#pragma pop_macro("TRUE")
#pragma pop_macro("NULL")