'use strict';

let BufferWriter = require('./buffer-writer');
let StringPool = require('./string-pool');
let ByteCode = require('./bytecode');

const Insts =
{
    RETURN : 0,         // <value>

    LOAD : 1,           // <addr>
    STORE : 2,          // <addr> <value>
    GET_ITEM : 3,       // <object> <key>
    SET_ITEM : 4,       // <object> <key> <value>
    DEL_ITEM : 5,       // <object> <key>
    NEW : 6,            // <callee> <args>
    CALL : 7,           // <callee> <this_arg> <args>

    JUMP : 8,           // <dst>
    BRANCH : 9,         // <condition> <consequence> <alternative>
    GET_GLOBAL : 10,    // <name>
    SET_GLOBAL : 11,    // <name> <value>

    UNDEFINED : 20,
    NULL : 21,
    TRUE : 22,
    FALSE : 23,
    NUMBER : 24,        // <value>
    STRING : 25,        // <stringPoolIndex>
    REGEX : 26,         // <regexLiteral>
    OBJECT : 27,        // <num> [<key> <value>]
    ARRAY : 28,         // <num> [<value>]
    FUNC : 29,          // <id>
    ARROW : 30,         // <id>

    ADD : 40,           // <a> <b>
    SUB : 41,           // <a> <b>
    MUL : 42,           // <a> <b>
    DIV : 43,           // <a> <b>
    MOD : 44,           // <a> <b>

    BAND : 45,          // <a> <b>
    BOR : 46,           // <a> <b>
    BXOR : 47,          // <a> <b>
    BNOT : 48,          // <a>

    LNOT : 49,          // <a>

    SLL : 50,           // <a> <b>
    SRL : 51,           // <a> <b>
    SRR : 52,           // <a> <b>

    EQ : 53,            // <a> <b>
    EQQ : 54,           // <a> <b>
    NE : 55,            // <a> <b>
    NEE : 56,           // <a> <b>
    GT : 57,            // <a> <b>
    GE : 58,            // <a> <b>
    LT : 59,            // <a> <b>
    LE : 60,            // <a> <b>

    IN : 61,            // <a> <b>
    INSTANTCEOF : 62,   // <a> <b>
    TYPEOF : 63,        // <a>

    PUSH_SCOPE : 70,    // <size>
    POP_SCOPE : 71,     //
    ALLOCA : 72,
    ARG : 73,
    CAPTURE: 74,

    THIS: 80,
    ARGUMENTS: 81,
    MOVE: 82,           // <other>

    PHI : 90,           // <num> [<branch> <value>]

    InstToString(inst, func) {
        if (inst.name === null)
        {
            inst.name = '.' + func.CountInst().toString();
        }

        let ret = `${inst.__offset}\t`;

        switch (inst.type)
        {
            case Insts.RETURN:
                return ret + `\t  return $${inst.$reg.name}`;
            case Insts.LOAD:
                return ret + `$${inst.name}\t= load $${inst.$addr.name}`;
            case Insts.STORE:
                return ret + `\t  store $${inst.$addr.name} $${inst.$value.name}`;
            case Insts.GET_ITEM:
                return ret + `$${inst.name}\t= get_item $${inst.$object.name} $${inst.$key.name}`;
            case Insts.SET_ITEM:
                return ret + `\t  set_item $${inst.$object.name} $${inst.$key.name} $${inst.$value.name}`;
            case Insts.DEL_ITEM:
                return ret + `\t  del_item $${inst.$object.name} $${inst.$key.name}`;
            case Insts.NEW:
                return ret + `$${inst.name}\t= new $${inst.$callee.name} $${inst.$args.name}`;
            case Insts.CALL:
                return ret + `$${inst.name}\t= call $${inst.$callee.name} $${inst.$this_arg.name} $${inst.$args.name}`;
            case Insts.JUMP:
                return ret + `\t  jump blk_${inst.dstBlock.index}`;
            case Insts.BRANCH:
                return ret + `\t  branch $${inst.$condition.name} blk_${inst.consequence.index} blk_${inst.alternative.index}`;
            case Insts.GET_GLOBAL:
                return ret + `$${inst.name}\t= get_global "${IRBuilder.EscapeString(inst.global)}"`;
            case Insts.SET_GLOBAL:
                return ret + `\t  set_global "${IRBuilder.EscapeString(inst.global)}" $${inst.$value.name}`;
            case Insts.UNDEFINED:
                return ret + `$${inst.name}\t= undefined`;
            case Insts.NULL:
                return ret + `$${inst.name}\t= null`;
            case Insts.TRUE:
                return ret + `$${inst.name}\t= true`;
            case Insts.FALSE:
                return ret + `$${inst.name}\t= false`;
            case Insts.NUMBER:
                return ret + `$${inst.name}\t= number ${inst.value}`;
            case Insts.STRING:
                return ret + `$${inst.name}\t= string "${IRBuilder.EscapeString(inst.value)}"`;
            case Insts.REGEX:
                return ret + `$${inst.name}\t= regex ${inst.value}`;
            case Insts.OBJECT:
                return ret + `$${inst.name}\t= object {${inst.initialize.map((i) => `$${i.$key.name}:$${i.$value.name}`).join(',')}}`;
            case Insts.ARRAY:
                return ret + `$${inst.name}\t= array [${inst.initialize.map((i) => '$' + i.name).join(',')}]`;
            case Insts.FUNC:
                return ret + `$${inst.name}\t= func [${inst.captured.map((i) => '$' + i.name).join(',')}] ${inst.func.id}`;
            case Insts.ARROW:
                return ret + `$${inst.name}\t= arrow [${inst.captured.map((i) => '$' + i.name).join(',')}] ${inst.func.id}`;
            case Insts.ADD:
                return ret + `$${inst.name}\t= add $${inst.$a.name} $${inst.$b.name}`;
            case Insts.SUB:
                return ret + `$${inst.name}\t= sub $${inst.$a.name} $${inst.$b.name}`;
            case Insts.MUL:
                return ret + `$${inst.name}\t= mul $${inst.$a.name} $${inst.$b.name}`;
            case Insts.DIV:
                return ret + `$${inst.name}\t= div $${inst.$a.name} $${inst.$b.name}`;
            case Insts.MOD:
                return ret + `$${inst.name}\t= mod $${inst.$a.name} $${inst.$b.name}`;
            case Insts.BAND:
                return ret + `$${inst.name}\t= band $${inst.$a.name} $${inst.$b.name}`;
            case Insts.BOR:
                return ret + `$${inst.name}\t= bor $${inst.$a.name} $${inst.$b.name}`;
            case Insts.BXOR:
                return ret + `$${inst.name}\t= bxor $${inst.$a.name} $${inst.$b.name}`;
            case Insts.BNOT:
                return ret + `$${inst.name}\t= bnot $${inst.$a.name}`;
            case Insts.LNOT:
                return ret + `$${inst.name}\t= lnot $${inst.$a.name}`;
            case Insts.SLL:
                return ret + `$${inst.name}\t= sll $${inst.$a.name} $${inst.$b.name}`;
            case Insts.SRL:
                return ret + `$${inst.name}\t= srl $${inst.$a.name} $${inst.$b.name}`;
            case Insts.SRR:
                return ret + `$${inst.name}\t= srr $${inst.$a.name} $${inst.$b.name}`;
            case Insts.EQ:
                return ret + `$${inst.name}\t= eq $${inst.$a.name} $${inst.$b.name}`;
            case Insts.EQQ:
                return ret + `$${inst.name}\t= eqq $${inst.$a.name} $${inst.$b.name}`;
            case Insts.NE:
                return ret + `$${inst.name}\t= ne $${inst.$a.name} $${inst.$b.name}`;
            case Insts.NEE:
                return ret + `$${inst.name}\t= nee $${inst.$a.name} $${inst.$b.name}`;
            case Insts.GT:
                return ret + `$${inst.name}\t= gt $${inst.$a.name} $${inst.$b.name}`;
            case Insts.GE:
                return ret + `$${inst.name}\t= ge $${inst.$a.name} $${inst.$b.name}`;
            case Insts.LT:
                return ret + `$${inst.name}\t= lt $${inst.$a.name} $${inst.$b.name}`;
            case Insts.LE:
                return ret + `$${inst.name}\t= le $${inst.$a.name} $${inst.$b.name}`;
            case Insts.IN:
                return ret + `$${inst.name}\t= in $${inst.$a.name} $${inst.$b.name}`;
            case Insts.INSTANTCEOF:
                return ret + `$${inst.name}\t= instanceof $${inst.$a.name} $${inst.$b.name}`;
            case Insts.TYPEOF:
                return ret + `$${inst.name}\t= typeof $${inst.$a.name}`;
            case Insts.PUSH_SCOPE:
                return ret + `\t  push_scope ${inst.size} [${inst.captured.map((c) => '$' + c.name).join(',')}]`;
            case Insts.POP_SCOPE:
                return ret + `\t  pop_scope ${inst.count}`;
            case Insts.ALLOCA:
                return ret + `$${inst.name}\t= alloca`;
            case Insts.ARG:
                return ret + `$${inst.name}\t= arg ${inst.index}`;
            case Insts.CAPTURE:
                return ret + `$${inst.name}\t= capture ${inst.index}`;
            case Insts.THIS:
                return ret + `$${inst.name}\t= this`;
            case Insts.ARGUMENTS:
                return ret + `$${inst.name}\t= arguments`;
            case Insts.MOVE:
                return ret + `$${inst.name}\t= $${inst.$other.name}`;
            case Insts.PHI:
                return ret + `$${inst.name}\t= phi ` + inst.branches.map((b) => `[blk_${b.precedence.index} $${b.$value.name}]`).join(' ')

            default:
                throw TypeError(`Invalid Inst type ${inst.type}`);
        }
    },

    DumpInst(inst, writer, stringPool)
    {
        writer.uint(inst.type);
        switch (inst.type)
        {
            case Insts.RETURN:
                writer.uint(inst.$reg.__offset);
                break;
            case Insts.LOAD:
                writer.uint(inst.$addr.__offset);
                break;
            case Insts.STORE:
                writer.uint(inst.$addr.__offset);
                writer.uint(inst.$value.__offset);
                break;
            case Insts.GET_ITEM:
                writer.uint(inst.$object.__offset);
                writer.uint(inst.$key.__offset);
                break;
            case Insts.SET_ITEM:
                writer.uint(inst.$object.__offset);
                writer.uint(inst.$key.__offset);
                writer.uint(inst.$value.__offset);
                break;
            case Insts.DEL_ITEM:
                writer.uint(inst.$object.__offset);
                writer.uint(inst.$key.__offset);
                break;
            case Insts.NEW:
                writer.uint(inst.$callee.__offset);
                writer.uint(inst.$args.__offset);
                break;
            case Insts.CALL:
                writer.uint(inst.$callee.__offset);
                writer.uint(inst.$this_arg.__offset);
                writer.uint(inst.$args.__offset);
                break;
            case Insts.JUMP:
                writer.uint(inst.dstBlock.index);
                break;
            case Insts.BRANCH:
                writer.uint(inst.$condition.__offset);
                writer.uint(inst.consequence.index);
                writer.uint(inst.alternative.index);
                break;
            case Insts.GET_GLOBAL:
                writer.uint(stringPool.Get(inst.global));
                break;
            case Insts.SET_GLOBAL:
                writer.uint(stringPool.Get(inst.global));
                writer.uint(inst.$value.__offset);
                break;
            case Insts.UNDEFINED:
            case Insts.NULL:
            case Insts.TRUE:
            case Insts.FALSE:
                break;
            case Insts.NUMBER:
                writer.double(inst.value);
                break;
            case Insts.STRING:
                writer.uint(stringPool.Get(inst.value));
                break;
            case Insts.REGEX:
                writer.uint(stringPool.Get(inst.value));
                break;
            case Insts.OBJECT:
                writer.uint(inst.initialize.length);
                for (let i of inst.initialize)
                {
                    writer.uint(i.$key.__offset);
                    writer.uint(i.$value.__offset);
                }
                break;
            case Insts.ARRAY:
                writer.uint(inst.initialize.length);
                for (let i of inst.initialize)
                {
                    writer.uint(i.__offset);
                }
                break;
            case Insts.FUNC:
                writer.uint(inst.func.id);
                writer.uint(inst.captured.length);
                for (let c of inst.captured)
                {
                    writer.uint(c.__offset);
                }
                break;
            case Insts.ARROW:
                writer.uint(inst.func.id);
                writer.uint(inst.captured.length);
                for (let c of inst.captured)
                {
                    writer.uint(c.__offset);
                }
                break;
            case Insts.ADD:
            case Insts.SUB:
            case Insts.MUL:
            case Insts.DIV:
            case Insts.MOD:
            case Insts.BAND:
            case Insts.BOR:
            case Insts.BXOR:
            case Insts.SLL:
            case Insts.SRL:
            case Insts.SRR:
            case Insts.EQ:
            case Insts.EQQ:
            case Insts.NE:
            case Insts.NEE:
            case Insts.GT:
            case Insts.GE:
            case Insts.LT:
            case Insts.LE:
            case Insts.IN:
            case Insts.INSTANTCEOF:
                writer.uint(inst.$a.__offset);
                writer.uint(inst.$b.__offset);
                break;
            case Insts.BNOT:
            case Insts.LNOT:
            case Insts.TYPEOF:
                writer.uint(inst.$a.__offset);
                break;
            case Insts.PUSH_SCOPE:
                writer.uint(inst.size);
                writer.uint(inst.captured.length);
                for (let c of inst.captured)
                {
                    writer.uint(c.__offset);
                }
                break;
            case Insts.POP_SCOPE:
                writer.uint(inst.count);
                break;
            case Insts.ALLOCA:
            case Insts.THIS:
            case Insts.ARGUMENTS:
                break;
            case Insts.ARG:
                writer.uint(inst.index);
                break;
            case Insts.CAPTURE:
                writer.uint(inst.index);
                break;
            case Insts.MOVE:
                writer.uint(inst.$other.__offset);
                break;
            case Insts.PHI:
                writer.uint(inst.branches.length);
                for (let b of inst.branches)
                {
                    writer.uint(b.precedence.index);
                    writer.uint(b.$value.__offset);
                }
                break;

            default:
                throw TypeError(`Invalid Inst type ${inst.type}`);
        }
    }
};

class BlockBuilder
{
    constructor(index, func)
    {
        this.index = index;
        this.insts = [];
        this.func = func;
    }

    PushInst(inst)
    {
        this.insts.push(inst);
        return inst;
    }

    PopInst()
    {
        return this.insts.pop();
    }

    LastInst()
    {
        return this.insts[this.insts.length - 1];
    }

    Return($reg)
    {
        return this.PushInst(
            {
                type : Insts.RETURN,
                $reg
            });
    }

    Load($addr, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.LOAD,
                $addr,
            });
    }

    Store($addr, $value)
    {
        return this.PushInst(
            {
                type : Insts.STORE,
                $addr, $value
            });
    }

    GetItem($object, $key, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.GET_ITEM,
                $object, $key
            });
    }

    SetItem($object, $key, $value)
    {
        return this.PushInst(
            {
                type : Insts.SET_ITEM,
                $object, $key, $value
            });
    }

    DelItem($object, $key)
    {
        return this.PushInst(
            {
                type : Insts.DEL_ITEM,
                $object, $key
            });
    }

    New($callee, $args, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.NEW,
                $callee, $args
            });
    }

    Call($callee, $this_arg, $args, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.CALL,
                $callee, $this_arg, $args
            });
    }

    Jump(dstBlock)
    {
        return this.PushInst(
            {
                type : Insts.JUMP,
                dstBlock
            });
    }

    Branch($condition, consequence, alternative)
    {
        return this.PushInst(
            {
                type : Insts.BRANCH,
                $condition, consequence, alternative
            });
    }

    GetGlobal(global, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.GET_GLOBAL,
                global
            });
    }

    SetGlobal(global, $value, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.SET_GLOBAL,
                global, $value
            });
    }

    Undefined(name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.UNDEFINED
            });
    }

    Null(name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.NULL
            });
    }

    True(name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.TRUE
            });
    }

    False(name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.FALSE
            });
    }

    Number(value, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.NUMBER,
                value
            });
    }

    String(value, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.STRING,
                value
            });
    }

    Object(initialize, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.OBJECT,
                initialize
            });
    }

    Array(initialize, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.ARRAY,
                initialize
            });
    }

    Func(func, captured, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.FUNC,
                func, captured
            });
    }

    Arrow(func, captured, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.ARROW,
                func, captured
            });
    }

    Binary(type, $a, $b, name = null)
    {
        return this.PushInst(
            {
                name,
                type, $a, $b
            });
    }

    Unary(type, $a, name = null)
    {
        return this.PushInst(
            {
                name,
                type, $a
            });
    }

    PushScope(size = 0)
    {
        return this.PushInst(
            {
                type : Insts.PUSH_SCOPE,
                size,
                captured : []
            });
    }

    PopScope(count = 1)
    {
        return this.PushInst(
            {
                type : Insts.POP_SCOPE,
                count
            });
    }

    Alloca(name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.ALLOCA
            });
    }

    Arg(index, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.ARG,
                index
            });
    }

    Capture(index, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.CAPTURE,
                index
            });
    }

    This(name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.THIS
            });
    }

    Arguments(name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.ARGUMENTS
            });
    }

    Move($other, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.MOVE,
                $other
            });
    }

    Phi(branches, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.PHI,
                branches
            });
    }

    toString(func, indent = '\t', newLine = '\n')
    {
        return `blk_${this.index}:` + newLine +
            IRBuilder.IncIndent(
                this.insts.map((i) =>
                    Insts.InstToString(i, func)).join(newLine),
                indent,
                newLine
            );
    }
};

let functionCounter = 0;
class FunctionBuilder
{
    constructor(name, length, ir)
    {
        this.name = name;
        this.length = length;
        this.blocks = [];
        this.ir = ir;
        this.localCounter = 0;
        this.id = functionCounter++;
    }

    NewBlock()
    {
        let block = new BlockBuilder(this.blocks.length, this);
        this.blocks.push(block);
        return block;
    }

    CountInst()
    {
        return this.localCounter++;
    }

    toString(indent = '\t', newLine = '\n')
    {
        return `function ${this.id} "${IRBuilder.EscapeString(this.name)}"` + newLine +
            IRBuilder.IncIndent(
                    this.blocks.map((b) => b.toString(this, indent, newLine)).join(newLine),
                indent,
                newLine
            );
    }

    /**
     * @param {BufferWriter} writer
     */
    Dump(writer, stringPool)
    {
        writer.uint(stringPool.Get(this.name));
        writer.uint(this.length);
        writer.uint(this.blocks.length);

        let __offset = 0;
        for (let block of this.blocks)
        {
            for (let inst of block.insts)
            {
                inst.__offset = __offset++;
            }
        }

        for (let block of this.blocks)
        {
            writer.uint(block.insts.length);
            for (let inst of block.insts)
            {
                Insts.DumpInst(inst, writer, stringPool);
            }
        }
    }
};

class IRBuilder
{
    constructor(name)
    {
        this.name = name;
        this.functions = [];
    }

    NewFunc(name, length)
    {
        let func = new FunctionBuilder(name, length, this);
        this.functions.push(func);
        return func;
    }

    toString(indent = '\t', newLine = '\n')
    {
        return `module "${IRBuilder.EscapeString(this.name)}"` + newLine +
            IRBuilder.IncIndent(
                this.functions.map((f) => f.toString(indent, newLine)).join(newLine + newLine),
                indent,
                newLine
            );
    }

    Dump()
    {
        let byteCode = new ByteCode();
        let stringPool = new StringPool();

        for (let func of this.functions)
        {
            let writer = new BufferWriter();
            func.Dump(writer, stringPool);
            byteCode.AddSection(writer, ByteCode.SECTION_TYPE.FUNCTION);
        }
        byteCode.AddSection(stringPool.writer, ByteCode.SECTION_TYPE.STRING_POOL);

        return byteCode;
    }

    static EscapeString(str)
    {
        return (str + '').replace(/[\\"']/g, '\\$&').replace(/\u0000/g, '\\0');
    }

    static IncIndent(multiLines, indent = '\t', newLine = '\n')
    {
        return multiLines.split(/\r?\n/g)
            .map((line) => indent + line)
            .join(newLine);
    }

    static DecIndent(multiLines, indent = '\t', newLine = '\n')
    {
        return multiLines.split(/\r|\n|(\r\n)/g)
            .map((line) => line.startsWith(indent) ? line.substr(indent.length) : line)
            .join(newLine);
    }

    static get Insts()
    {
        return Insts;
    }
};

module.exports = IRBuilder;
