'use strict';

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
    GET_GLOBAL : 10,     // <name>

    UNDEFINED : 11,
    NULL : 12,
    TRUE : 13,
    FALSE : 14,
    NUMBER : 15,        // <value>
    STRING : 16,        // <stringPoolIndex>
    REGEX : 17,         // <regexLiteral>
    OBJECT : 18,        // <num> [<key> <value>]
    ARRAY : 19,         // <num> [<value>]
    FUNC : 20,          // <id>

    ADD : 21,           // <a> <b>
    SUB : 22,           // <a> <b>
    MUL : 23,           // <a> <b>
    DIV : 24,           // <a> <b>
    MOD : 25,           // <a> <b>

    BAND : 26,          // <a> <b>
    BOR : 27,           // <a> <b>
    BXOR : 28,          // <a> <b>
    BNOT : 29,          // <a>

    LNOT : 31,          // <a>

    SLL : 32,           // <a> <b>
    SRL : 33,           // <a> <b>
    SRR : 34,           // <a> <b>

    EQ : 35,            // <a> <b>
    EQQ : 36,           // <a> <b>
    NE : 37,            // <a> <b>
    NEE : 38,           // <a> <b>
    GT : 39,            // <a> <b>
    GE : 40,            // <a> <b>
    LT : 41,            // <a> <b>
    LE : 42,            // <a> <b>

    IN : 43,            // <a> <b>
    INSTANTCEOF : 44,   // <a> <b>
    TYPEOF : 45,        // <a>

    PUSH_SCOPE : 46,    // <size>
    POP_SCOPE : 47,     //
    ALLOCA : 48,
    ARG : 49,
    CAPTURE: 50,

    THIS: 51,
    MOVE: 52,           // <other>

    PHI : 53,           // <num> [<branch> <value>]

    InstToString(inst, func) {
        if (inst.name === null)
        {
            inst.name = '.' + func.CountInst().toString();
        }

        switch (inst.type)
        {
            case Insts.RETURN:
                return `\t  return $${inst.$reg.name}`;
            case Insts.LOAD:
                return `$${inst.name}\t= load $${inst.$addr.name}`;
            case Insts.STORE:
                return `\t  store $${inst.$addr.name} $${inst.$value.name}`;
            case Insts.GET_ITEM:
                return `$${inst.name}\t= get_item $${inst.$object.name} $${inst.$key.name}`;
            case Insts.SET_ITEM:
                return `\t  set_item $${inst.$object.name} $${inst.$key.name} $${inst.$value.name}`;
            case Insts.DEL_ITEM:
                return `\t  del_item $${inst.$object.name} $${inst.$key.name}`;
            case Insts.NEW:
                return `$${inst.name}\t= new $${inst.$callee.name} $${inst.$args.name}`;
            case Insts.CALL:
                return `$${inst.name}\t= call $${inst.$callee.name} $${inst.$this_arg.name} $${inst.$args.name}`;
            case Insts.JUMP:
                return `\t  jump blk_${inst.dstBlock.index}`;
            case Insts.BRANCH:
                return `\t  branch $${inst.$condition.name} blk_${inst.consequence.index} blk_${inst.alternative.index}`;
            case Insts.GET_GLOBAL:
                return `$${inst.name}\t= get_global "${IRBuilder.EscapeString(inst.global)}"`;
            case Insts.UNDEFINED:
                return `$${inst.name}\t= undefined`;
            case Insts.NULL:
                return `$${inst.name}\t= null`;
            case Insts.TRUE:
                return `$${inst.name}\t= true`;
            case Insts.FALSE:
                return `$${inst.name}\t= false`;
            case Insts.NUMBER:
                return `$${inst.name}\t= number ${inst.value}`;
            case Insts.STRING:
                return `$${inst.name}\t= string "${IRBuilder.EscapeString(inst.value)}"`;
            case Insts.REGEX:
                return `$${inst.name}\t= regex ${inst.value}`;
            case Insts.OBJECT:
                return `$${inst.name}\t= object {${inst.initialize.map((i) => `$${i.$key.name}:$${i.$value.name}`).join(',')}}`;
            case Insts.ARRAY:
                return `$${inst.name}\t= array [${inst.initialize.map((i) => '$' + i.name).join(',')}]`;
            case Insts.FUNC:
                return `$${inst.name}\t= func [${inst.captured.map((i) => '$' + i.name).join(',')}] ${inst.func.name}`;
            case Insts.ADD:
                return `$${inst.name}\t= add $${inst.$a.name} $${inst.$b.name}`;
            case Insts.SUB:
                return `$${inst.name}\t= sub $${inst.$a.name} $${inst.$b.name}`;
            case Insts.MUL:
                return `$${inst.name}\t= mul $${inst.$a.name} $${inst.$b.name}`;
            case Insts.DIV:
                return `$${inst.name}\t= div $${inst.$a.name} $${inst.$b.name}`;
            case Insts.MOD:
                return `$${inst.name}\t= mod $${inst.$a.name} $${inst.$b.name}`;
            case Insts.BAND:
                return `$${inst.name}\t= band $${inst.$a.name} $${inst.$b.name}`;
            case Insts.BOR:
                return `$${inst.name}\t= bor $${inst.$a.name} $${inst.$b.name}`;
            case Insts.BXOR:
                return `$${inst.name}\t= bxor $${inst.$a.name} $${inst.$b.name}`;
            case Insts.BNOT:
                return `$${inst.name}\t= bnot $${inst.$a.name}`;
            case Insts.LNOT:
                return `$${inst.name}\t= lnot $${inst.$a.name}`;
            case Insts.SLL:
                return `$${inst.name}\t= sll $${inst.$a.name} $${inst.$b.name}`;
            case Insts.SRL:
                return `$${inst.name}\t= srl $${inst.$a.name} $${inst.$b.name}`;
            case Insts.SRR:
                return `$${inst.name}\t= srr $${inst.$a.name} $${inst.$b.name}`;
            case Insts.EQ:
                return `$${inst.name}\t= eq $${inst.$a.name} $${inst.$b.name}`;
            case Insts.EQQ:
                return `$${inst.name}\t= eqq $${inst.$a.name} $${inst.$b.name}`;
            case Insts.NE:
                return `$${inst.name}\t= ne $${inst.$a.name} $${inst.$b.name}`;
            case Insts.NEE:
                return `$${inst.name}\t= nee $${inst.$a.name} $${inst.$b.name}`;
            case Insts.GT:
                return `$${inst.name}\t= gt $${inst.$a.name} $${inst.$b.name}`;
            case Insts.GE:
                return `$${inst.name}\t= ge $${inst.$a.name} $${inst.$b.name}`;
            case Insts.LT:
                return `$${inst.name}\t= lt $${inst.$a.name} $${inst.$b.name}`;
            case Insts.LE:
                return `$${inst.name}\t= le $${inst.$a.name} $${inst.$b.name}`;
            case Insts.IN:
                return `$${inst.name}\t= in $${inst.$a.name} $${inst.$b.name}`;
            case Insts.INSTANTCEOF:
                return `$${inst.name}\t= instanceof $${inst.$a.name} $${inst.$b.name}`;
            case Insts.TYPEOF:
                return `$${inst.name}\t= typeof $${inst.$a.name}`;
            case Insts.PUSH_SCOPE:
                return `\t  push_scope ${inst.size}`;
            case Insts.POP_SCOPE:
                return `\t  pop_scope ${inst.count}`;
            case Insts.ALLOCA:
                return `$${inst.name}\t= alloca`;
            case Insts.ARG:
                return `$${inst.name}\t= arg $${inst.$index.name}`;
            case Insts.CAPTURE:
                return `$${inst.name}\t= capture ${inst.index}`;
            case Insts.THIS:
                return `$${inst.name}\t= this`;
            case Insts.MOVE:
                return `$${inst.name}\t= $${inst.$other.name}`;
            case Insts.PHI:
                return `$${inst.name}\t= phi ` + inst.branches.map((b) => `[blk_${b.precedence.index} $${b.$value.name}]`).join(' ')

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
                capture : []
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

    Arg($index, name = null)
    {
        return this.PushInst(
            {
                name,
                type : Insts.ARG,
                $index
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

class FunctionBuilder
{
    constructor(name, ir)
    {
        this.name = name;
        this.blocks = [];
        this.ir = ir;
        this.localCounter = 0;
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
        return `function "${IRBuilder.EscapeString(this.name)}"` + newLine +
            IRBuilder.IncIndent(
                    this.blocks.map((b) => b.toString(this, indent, newLine)).join(newLine),
                indent,
                newLine
            );
    }
};

class IRBuilder
{
    constructor(name)
    {
        this.name = name;
        this.functions = [];
    }

    NewFunc(name)
    {
        let func = new FunctionBuilder(name, this);
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
