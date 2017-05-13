'use strict';

let IRBuilder = require('./ir-builder.js');

class Scope
{
    /**
     * @param {Scope} upper
     */
    constructor(upper, breakable = null, continuable = null, func = null)
    {
        this.map = {};
        this.capture = {};
        this.captureCount = 0;

        this.args = {};
        this.restArg = null;

        this.breakable = breakable;
        this.continuable = continuable;

        this.upper = upper;
        this.func = func;
    }

    SetArguments(args, restArg)
    {
        this.args = args;
        this.restArg = restArg;
    }

    /**
     * @param {string} name 
     * @returns {{index : number, type : 'direct'|'capture'|'args', inst : number}}
     */
    Lookup(name, tolerantForwardDeclare = false, upperTolerantForwardDeclare = true)
    {
        if (this.args.hasOwnProperty(name))
        {
            return {
                index : this.args[name],
                type : 'args'
            };
        }
        else if (this.restArg === name)
        {
            return {
                index : -1,
                type : 'restArg'
            };
        }
        else if (this.map.hasOwnProperty(name))
        {
            if (!tolerantForwardDeclare && this.map[name].type === 'forward')
            {
                throw SyntaxError(`Identifier '${name}' is not defined`);
            }

            if (!this.map[name].inst)
            {
                throw Error('Internal');
            }

            return {
                inst : this.map[name].inst,
                type : 'direct'
            };
        }
        else if (this.capture.hasOwnProperty(name))
        {
            return {
                index : this.capture[name],
                type : 'capture'
            };
        }
        else if (this.upper)
        {
            let upperIndex = this.upper.Lookup(name, upperTolerantForwardDeclare);
            if (upperIndex !== null)
            {
                this.capture[name] = this.captureCount++;
                return {
                    index : this.capture[name],
                    type : 'capture'
                };
            }
        }

        return null;
    }

    ForwardDeclare(name, inst)
    {
        if (this.map.hasOwnProperty(name))
        {
            throw SyntaxError(`Identifier '${name}' has been already defined`);
        }

        this.map[name] = {
            inst,
            type : 'forward'
        };
    }

    Declare(name, inst = null)
    {
        if (this.map.hasOwnProperty(name))
        {
            if (this.map[name].type !== 'forward')
            {
                throw SyntaxError(`Identifier '${name}' has been already defined`);
            }

            this.map[name].type = 'let';
            if (inst)
            {
                this.map[name].inst = inst;
            }

            return;
        }

        this.map[name] = {
            inst,
            type : 'let'
        };
    }

    DeclareFunc(name, inst)
    {
        if (this.map.hasOwnProperty(name))
        {
            if (this.map[name].type !== 'forward')
            {
                throw SyntaxError(`Identifier '${name}' has been already defined`);
            }
        }

        this.map[name] = {
            inst,
            type : 'let'
        };
    }

    VariableCount()
    {
        return Object.getOwnPropertyNames(this.map).length;
    }

    CaptureList()
    {
        return Object.getOwnPropertyNames(this.capture)
            .map((name) => { return { name, index : this.capture[name] }; })
            .sort((a, b) => a.index - b.index);
    }
};

// return last block, the last inst is the result of expression 
function CompileExpression(node, func, last, scope)
{
    switch (node.type)
    {
        case 'ThisExpression':
            {
                last.This();
            }
            break;
        case 'Identifier':
            {
                let result = scope.Lookup(node.name);
                if (!result)
                {
                    let $global = last.GetGlobal(node.name);
                    last.Load($global);
                }
                else if (result.type === 'capture')
                {
                    let $cap = last.Capture(result.index);
                    last.Load($cap);
                }
                else if (result.type === 'direct')
                {
                    last.Load(result.inst);
                }
                else if (result.type === 'args')
                {
                    let $arg = last.Arg(result.index);
                    last.Load($arg);
                }
                else
                {
                    throw Error('Internal');
                }
            }
            break;
        case 'Literal':
            {
                switch (typeof node.value)
                {
                    case 'boolean':
                        if (node.value)
                        {
                            last.True();
                        }
                        else
                        {
                            last.False();
                        }
                        break;
                    case 'number':
                        last.Number(node.value);
                        break;
                    case 'string':
                        last.String(node.value);
                        break;
                    case 'object':
                        // regex;
                }
            }
            break;
        case 'ArrayExpression':
            {
                let compileValueRange = (elements) =>
                {
                    let values = elements.map((ele) => {
                        last = CompileExpression(ele, func, last, scope);
                        return last.LastInst();
                    });

                    last.Array(values);
                    return last.LastInst();
                };

                let compileArrayConcat = (left, arrays) =>
                {
                    let $Array = last.GetGlobal('Array');
                    let $prototype = last.String('prototype');
                    let $Array_prototype = last.GetItem($Array, $prototype);
                    let $concat = last.String('concat');
                    let $Array_prototype_concat = last.GetItem($Array_prototype, $concat);
                    last.Array(arrays);
                    last.Call($Array_prototype_concat, left, last.LastInst());
                    return last.LastInst();
                };

                let compileArrayFromSpread = (ele) =>
                {
                    last = CompileExpression(ele.argument, func, last, scope);
                    let $arrayLike = last.LastInst();

                    let $Array = last.GetGlobal('Array');
                    let $from = last.String('from');
                    let $Array_from = last.GetItem($Array, $from);
                    last.Array([$arrayLike]);
                    last.Call($Array_from, $Array, last.LastInst());
                    return last.LastInst();
                };

                if (node.elements.some((e) => e.type === 'SpreadElement'))
                {
                    let currentValues = [];
                    let arrays = [];
                    for (let ele of node.elements)
                    {
                        if (ele.type === 'SpreadElement')
                        {
                            if (currentValues.length !== 0)
                            {
                                arrays.push(compileValueRange(currentValues));
                                currentValues = [];
                            }

                            arrays.push(compileArrayFromSpread(ele));
                        }
                        else
                        {
                            currentValues.push(ele);
                        }
                    }

                    if (currentValues.length !== 0)
                    {
                        arrays.push(compileValueRange(currentValues));
                    }

                    if (arrays.length <= 1)
                    {
                        throw Error("Internal");
                    }

                    compileArrayConcat(arrays[0], arrays.slice(1));
                }
                else
                {
                    compileValueRange(node.elements);
                }
            }
            break;
        case 'ObjectExpression':
            {
                let initialize = [];

                node.properties.forEach((p) =>
                {
                    if (p.kind !== 'init')
                    {
                        throw Error(`Property kind ${p.kind} is not supported`);
                    }

                    if (p.method)
                    {
                        throw Error('Property method is not supported');
                    }

                    let $key, $value;

                    if (p.computed)
                    {
                        last = CompileExpression(p.key, func, last, scope);
                        $key = last.LastInst();

                        last = CompileExpression(p.value, func, last, scope);
                        $value = last.LastInst();
                    }
                    else
                    {
                        if (p.key.type === 'Identifier') {
                            last.String(p.key.name);
                        }
                        else if (p.key.type === 'Literal') {
                            last.String(p.key.value.toString());
                        }
                        else {
                            throw Error('Internal');
                        }

                        $key = last.LastInst();

                        last = CompileExpression(p.value, func, last, scope);
                        $value = last.LastInst();
                    }

                    initialize.push({$key, $value});
                });

                last.Object(initialize);
            }
            break;
        case 'FunctionExpression':
            {
                let ret = CompileFunction(node, func.ir, scope);
                let captured = ret.captured.map((c) => {
                    let result = scope.Lookup(c.name);
                    if (!result)
                    {
                        throw Error('Internal');
                    }
                    else if (result.type === 'direct')
                    {
                        return result.inst;
                    }
                    else if (result.type === 'capture')
                    {
                        return last.Capture(result.index);
                    }
                    else if (result.type === 'args')
                    {
                        return last.Arg(result.index);
                    }
                })
                last.Func(ret.func, captured);
            }
            break;
        case 'ArrowFunctionExpression':
            {
                let ret = CompileArrowFunction(node, func.ir, scope);
                let captured = ret.captured.map((c) => {
                    let result = scope.Lookup(c.name);
                    if (!result)
                    {
                        throw Error('Internal');
                    }
                    else if (result.type === 'direct')
                    {
                        return result.inst;
                    }
                    else if (result.type === 'capture')
                    {
                        return last.Capture(result.index);
                    }
                    else if (result.type === 'args')
                    {
                        return last.Arg(result.index);
                    }
                })
                last.Arrow(ret.func, captured);

            }
            break;
        case 'ClassExpression':
        case 'TaggedExpression':
            throw Error('Not implemented');
        case 'MemberExpression':
            {
                let $obj, $key;
                last = CompileExpression(node.object, func, last, scope);
                $obj = last.LastInst();
                if (node.computed)
                {
                    last = CompileExpression(node.property, func, last, scope);
                    $key = last.LastInst();
                }
                else
                {
                    if (node.property.type !== 'Identifier')
                    {
                        throw Error('Internal');
                    }
                    $key = last.String(node.property.name);
                }

                last.GetItem($obj, $key);
            }
            break;
        case 'Super':
        case 'MetaProperty':
            throw Error('Not implemented');
        case 'NewExpression':
            {
                last = CompileExpression(node.callee, func, last, scope);
                let $callee = last.LastInst();

                let args = node.arguments.map((arg) =>
                {
                    if (arg.type === 'SpreadExpression')
                    {
                        throw Error('SpreadExpression in new expression is not supported');
                    }

                    last = CompileExpression(arg, func, last, scope);
                    return last.LastInst();
                });

                let $args = last.Array(args);

                last.New($callee, $args);
            }
            break;
        case 'CallExpression':
            {
                let $callee;
                let $this_arg;
                if (node.callee.type === 'MemberExpression' && !node.callee.computed)
                {
                    last = CompileExpression(node.callee.object, func, last, scope);
                    $this_arg = last.LastInst();

                    if (node.callee.property.type !== 'Identifier')
                    {
                        throw Error('Internal');
                    }
                    let $key = last.String(node.callee.property.name);

                    $callee = last.GetItem($this_arg, $key);
                }
                else
                {
                    last = CompileExpression(node.callee, func, last, scope);
                    $callee = last.LastInst();

                    $this_arg = last.GetGlobal("global");
                }

                let args = node.arguments.map((arg) =>
                {
                    if (arg.type === 'SpreadExpression')
                    {
                        throw Error('SpreadExpression in call expression is not supported');
                    }

                    last = CompileExpression(arg, func, last, scope);
                    return last.LastInst();
                });

                let $args = last.Array(args);

                last.Call($callee, $this_arg, $args);
            }
            break;
        case 'UpdateExpression':
            {
                if (node.argument.type === 'Identifier')
                {
                    let $addr;
                    let result = scope.Lookup(node.argument.name);
                    if (!result)
                    {
                        $addr = last.GetGlobal(node.argument.name);
                    }
                    else if (result.type === 'capture')
                    {
                        $addr = last.Capture(result.index);
                    }
                    else if (result.type === 'direct')
                    {
                        $addr = result.inst;
                    }
                    else if (result.type === 'args')
                    {
                        $addr = last.Arg(result.index);
                    }
                    else
                    {
                        throw Error('Internal');
                    }

                    let $value = last.Load($addr);
                    let $one = last.Number(1);
                    let $newValue;
                    if (node.operator === '++')
                    {
                        $newValue = last.Binary(IRBuilder.Insts.ADD, $value, $one);
                    }
                    else
                    {
                        $newValue = last.Binary(IRBuilder.Insts.SUB, $value, $one);
                    }
                    last.Store($addr, $newValue);

                    if (node.prefix)
                    {
                        last.Move($newValue);
                    }
                    else
                    {
                        last.Move($value);
                    }
                }
                else if (node.argument.type === 'MemberExpression')
                {
                    let $obj, $key;
                    last = CompileExpression(node.argument.object, func, last, scope);
                    $obj = last.LastInst();
                    if (node.argument.computed)
                    {
                        last = CompileExpression(node.argument.property, func, last, scope);
                        $key = last.LastInst();
                    }
                    else
                    {
                        if (node.argument.property.type !== 'Identifier')
                        {
                            throw Error('Internal');
                        }
                        $key = last.String(node.argument.property.name);
                    }

                    let $value = last.GetItem($obj, $key);
                    let $one = last.Number(1);
                    let $newValue;
                    if (node.operator === '++')
                    {
                        $newValue = last.Binary(IRBuilder.Insts.ADD, $value, $one);
                    }
                    else
                    {
                        $newValue = last.Binary(IRBuilder.Insts.SUB, $value, $one);
                    }

                    last.SetItem($obj, $key, $newValue);

                    if (node.prefix)
                    {
                        last.Move($newValue);
                    }
                    else
                    {
                        last.Move($value);
                    }
                }
                else
                {
                    throw Error('Internal');
                }
            }
            break;
        case 'AwaitExpression':
        case 'UnaryExpression':
            {
                if (node.operator === 'delete')
                {
                    if (node.argument.type !== 'MemberExpression')
                    {
                        throw Error(`Cannot delete a ${node.argument.type}`);
                    }

                    let $obj, $key;
                    last = CompileExpression(node.argument.object, func, last, scope);
                    $obj = last.LastInst();

                    if (node.computed)
                    {
                        last = CompileExpression(node.argument.property, func, last, scope);
                        $key = last.LastInst();
                    }
                    else
                    {
                        if (node.property.type !== 'Identifier')
                        {
                            throw Error('Internal');
                        }
                        $key = last.String(node.property.name);
                    }

                    last.DelItem($obj, $key);
                }
                else
                {
                    last = CompileExpression(node.argument, func, last, scope);
                    let $operand = last.LastInst();

                    switch(node.operator)
                    {
                        case '+':
                        case '-':
                            {
                                let $left = last.Number(0);
                                if (node.operator === '+')
                                {
                                    last.Binary(IRBuilder.Insts.ADD, $left, $operand);
                                }
                                else
                                {
                                    last.Binary(IRBuilder.Insts.SUB, $left, $operand);
                                }
                            }
                            break;
                        case '~':
                            last.Unary(IRBuilder.Insts.BNOT, $operand);
                            break;
                        case '!':
                            last.Unary(IRBuilder.Insts.LNOT, $operand);
                            break;
                        case 'void':
                            last.Undefined();
                            break;
                        case 'typeof':
                            last.Unary(IRBuilder.Insts.TYPEOF, $operand);
                            break;
                        default:
                            throw Error('Internal');
                    }
                }
            }
            break;
        case 'BinaryExpression':
            {
                last = CompileExpression(node.left, func, last, scope);
                let $left = last.LastInst();

                last = CompileExpression(node.right, func, last, scope);
                let $right = last.LastInst();

                switch (node.operator)
                {
                    case 'instanceof':
                        last.Binary(IRBuilder.Insts.INSTANTCEOF, $left, $right);
                        break;
                    case 'in':
                        last.Binary(IRBuilder.Insts.IN, $left, $right);
                        break;
                    case '+':
                        last.Binary(IRBuilder.Insts.ADD, $left, $right);
                        break;
                    case '-':
                        last.Binary(IRBuilder.Insts.SUB, $left, $right);
                        break;
                    case '*':
                        last.Binary(IRBuilder.Insts.MUL, $left, $right);
                        break;
                    case '/':
                        last.Binary(IRBuilder.Insts.DIV, $left, $right);
                        break;
                    case '%':
                        last.Binary(IRBuilder.Insts.MOD, $left, $right);
                        break;
                    case '|':
                        last.Binary(IRBuilder.Insts.BOR, $left, $right);
                        break;
                    case '^':
                        last.Binary(IRBuilder.Insts.BXOR, $left, $right);
                        break;
                    case '&':
                        last.Binary(IRBuilder.Insts.BAND, $left, $right);
                        break;
                    case '==':
                        last.Binary(IRBuilder.Insts.EQ, $left, $right);
                        break;
                    case '!=':
                        last.Binary(IRBuilder.Insts.NE, $left, $right);
                        break;
                    case '===':
                        last.Binary(IRBuilder.Insts.EQQ, $left, $right);
                        break;
                    case '!==':
                        last.Binary(IRBuilder.Insts.NEE, $left, $right);
                        break;
                    case '<':
                        last.Binary(IRBuilder.Insts.LT, $left, $right);
                        break;
                    case '>':
                        last.Binary(IRBuilder.Insts.GT, $left, $right);
                        break;
                    case '<=':
                        last.Binary(IRBuilder.Insts.LE, $left, $right);
                        break;
                    case '>=':
                        last.Binary(IRBuilder.Insts.GE, $left, $right);
                        break;
                    case '<<':
                        last.Binary(IRBuilder.Insts.SLL, $left, $right);
                        break;
                    case '>>':
                        last.Binary(IRBuilder.Insts.SRL, $left, $right);
                        break;
                    case '>>>':
                        last.Binary(IRBuilder.Insts.SRR, $left, $right);
                        break;
                    default:
                        throw Error('Internal');
                }
            }
            break;
        case 'LogicalExpression':
            {
                let leftBlock = last;
                let rightBlock = func.NewBlock();
                let followingBlock = func.NewBlock();

                leftBlock = CompileExpression(node.left, func, leftBlock, scope);
                let $left = leftBlock.LastInst();

                if (node.operator === '&&')
                {
                    leftBlock.Branch($left, rightBlock, followingBlock);
                }
                else
                {
                    leftBlock.Branch($left, followingBlock, rightBlock);
                }

                rightBlock = CompileExpression(node.right, func, rightBlock, scope);
                let $right = rightBlock.LastInst();
                rightBlock.Jump(followingBlock);

                followingBlock.Phi([
                    {
                        precedence : leftBlock,
                        $value : $left
                    },
                    {
                        precedence : rightBlock,
                        $value : $right
                    }
                ]);

                last = followingBlock;
            }
            break;
        case 'ConditionalExpression':
            {
                last = CompileExpression(node.test, func, last, scope);
                let $test = last.LastInst();

                let leftBlock = func.NewBlock();
                let rightBlock = func.NewBlock();

                last.Branch($test, leftBlock, rightBlock);

                leftBlock = CompileExpression(node.consequent, func, leftBlock, scope);
                rightBlock = CompileExpression(node.alternate, func, rightBlock, scope);

                let $left = leftBlock.LastInst();
                let $right = rightBlock.LastInst();

                let followingBlock = func.NewBlock();
                followingBlock.Phi([
                    {
                        precedence : leftBlock,
                        $value : $left
                    },
                    {
                        precedence : rightBlock,
                        $value : $right
                    }
                ]);

                last = followingBlock;
            }
            break;
        case 'AssignmentExpression':
            {
                let isIdentifier = false;
                if (node.left.type === 'Identifier')
                {
                    isIdentifier = true;
                }
                else if (node.left.type === 'MemberExpression')
                {
                    isIdentifier = false;
                }
                else
                {
                    throw Error(`Cannot assign to a ${node.left.type}`);
                }

                last = CompileExpression(node.right, func, last, scope);
                let $right = last.LastInst();

                let $value, $newValue;
                let $addr, $obj, $key;
                if (isIdentifier)
                {
                    let result = scope.Lookup(node.left.name);
                    if (!result)
                    {
                        $addr = last.GetGlobal(node.left.name);
                    }
                    else if (result.type === 'capture')
                    {
                        $addr = last.Capture(result.index);
                    }
                    else if (result.type === 'direct')
                    {
                        $addr = result.inst;
                    }
                    else if (result.type === 'args')
                    {
                        $addr = last.Arg(result.index);
                    }
                    else
                    {
                        throw Error('Internal');
                    }
                }
                else
                {
                    last = CompileExpression(node.left.object, func, last, scope);
                    $obj = last.LastInst();
                    if (node.left.computed)
                    {
                        last = CompileExpression(node.argument.property, func, last, scope);
                        $key = last.LastInst();
                    }
                    else
                    {
                        if (node.argument.property.type !== 'Identifier')
                        {
                            throw Error('Internal');
                        }
                        $key = last.String(node.argument.property.name);
                    }
                }

                if (node.operator === '=')
                {
                    if (isIdentifier)
                    {
                        last.Store($addr, $right);
                    }
                    else
                    {
                        last.SetItem($obj, $key, $right);
                    }
                    last.Move($right);
                    break;
                }

                if (isIdentifier)
                {
                    $value = last.Load($addr);
                }
                else
                {
                    $value = last.GetItem($obj, $key);
                }

                switch (node.operator)
                {
                    case "+=":
                        $newValue = last.Binary(IRBuilder.Insts.ADD, $value, $right);
                        break;
                    case "-=":
                        $newValue = last.Binary(IRBuilder.Insts.SUB, $value, $right);
                        break;
                    case "*=":
                        $newValue = last.Binary(IRBuilder.Insts.MUL, $value, $right);
                        break;
                    case "/=":
                        $newValue = last.Binary(IRBuilder.Insts.DIV, $value, $right);
                        break;
                    case "%=":
                        $newValue = last.Binary(IRBuilder.Insts.MOD, $value, $right);
                        break;
                    case "<<=":
                        $newValue = last.Binary(IRBuilder.Insts.SLL, $value, $right);
                        break;
                    case ">>=":
                        $newValue = last.Binary(IRBuilder.Insts.SRL, $value, $right);
                        break;
                    case ">>>=":
                        $newValue = last.Binary(IRBuilder.Insts.SRR, $value, $right);
                        break;
                    case "&=":
                        $newValue = last.Binary(IRBuilder.Insts.BAND, $value, $right);
                        break;
                    case "|=":
                        $newValue = last.Binary(IRBuilder.Insts.BOR, $value, $right);
                        break;
                    case "^=":
                        $newValue = last.Binary(IRBuilder.Insts.BXOR, $value, $right);
                        break;
                    default:
                        throw Error('Internal');
                }

                if (isIdentifier)
                {
                    last.Store($addr, $newValue);
                }
                else
                {
                    last.SetItem($obj, $key, $newValue);
                }
                last.Move($newValue);
            }
            break;
        case 'SequenceExpression':
            {
                for (let expr of node.expressions)
                {
                    last = CompileExpression(expr, func, last, scope);
                }
            }
            break;
        case 'YieldExpression':
            throw Error('Not implemented');
    }
    return last;
}

function CompileStatement(node, func, last, scope)
{
    function PreparePushScope()
    {
        let next = func.NewBlock();
        last.Jump(next);
        return {
            last,
            next
        };
    }

    function CompletePushScope(lastScope, blockScope, last, $pushScope)
    {
        $pushScope.size = blockScope.VariableCount();

        let $jump = last.PopInst();

        for (let cap of blockScope.CaptureList())
        {
            let localSymbol = scope.Lookup(cap.name, true);
            if (localSymbol.type === 'direct')
            {
                $pushScope.captured.push(localSymbol.inst);
            }
            else if (localSymbol.type === 'capture')
            {
                let $cap = last.Capture(localSymbol.index);
                $pushScope.captured.push($cap);
            }
            else if (localSymbol.type === 'args')
            {
                let $arg = last.Arg(localSymbol.index);
                $pushScope.captured.push($arg);
            }
            else
            {
                throw Error('Internal');
            }
        }

        last.PushInst($jump);
    }

    switch (node.type)
    {
        case 'BlockStatement':
            {
                let blockScope = new Scope(scope);

                let {last, next} = PreparePushScope();
                let $pushScope = next.PushScope();

                let range = CompileNodeList(node.body, func, blockScope, next);
                CompletePushScope(scope, blockScope, last, $pushScope);

                next.Jump(range.entry);
                last = range.last;
                last.PopScope();

                return last;
            }
        case 'BreakStatement':
            {
                let breakableScope = scope;
                let popCount = 1;
                while (breakableScope !== null && !breakableScope.breakable)
                {
                    breakableScope = breakableScope.upper;
                    popCount ++;
                }
                if (breakableScope === null)
                {
                    throw SyntaxError('Not breakable');
                }
                last.PopScope(popCount);
                last.Jump(breakableScope.breakable);

                last = func.NewBlock();

                return last;
            }
        case 'ClassDeclaration':
            throw Error('Not implemented');
        case 'ContinueStatement':
            {
                let continuableScope = scope;
                let popCount = 1;
                while (continuableScope !== null && !continuableScope.continuable)
                {
                    continuableScope = continuableScope.upper;
                    popCount ++;
                }
                if (continuableScope === null)
                {
                    throw SyntaxError('Not continuable');
                }
                last.PopScope(popCount);
                last.Jump(continuableScope.continuable);

                return func.NewBlock();
            }
        case 'DebuggerStatement':
            break;
        case 'DoWhileStatement':
            {
                let loopEntry = func.NewBlock();
                let loopCmp = func.NewBlock();
                let loopBreak = func.NewBlock();

                let loopScope = new Scope(scope, loopBreak, loopCmp);
                let {last, next} = PreparePushScope();
                let $pushScope = next.PushScope(0);
                next.Jump(loopEntry);

                let loopBody = CompileStatement(node.body, func, loopEntry, loopScope);
                loopBody.Jump(loopCmp);

                loopCmp = CompileExpression(node.test, func, loopCmp, loopScope);
                let $cond = loopCmp.LastInst();
                loopCmp.Branch($cond, loopEntry, loopBreak);

                CompletePushScope(scope, loopScope, last, $pushScope);

                loopBreak.PopScope();
                return loopBreak;
            }
        case 'EmptyStatement':
            break;
        case 'ExpressionStatement':
            return CompileExpression(node.expression, func, last, scope);
        case 'ForStatement':
            {
                let loopEntry = func.NewBlock();
                let loopCmp = func.NewBlock();
                let loopBreak = func.NewBlock();
                let loopUpdate = func.NewBlock();

                let loopScope = new Scope(scope, loopBreak, loopCmp);
                let {last, next} = PreparePushScope();
                let $pushScope = next.PushScope(0);

                if (node.init)
                {
                    if (node.init.type === 'VariableDeclaration')
                    {
                        for (let decl of node.init.declarations)
                        {
                            if (decl.id.type !== 'Identifier')
                            {
                                throw Error('Not support pattern here');
                            }
                            loopScope.ForwardDeclare(decl.id.name, next.Alloca(decl.id.name));
                        }
                        next = CompileStatement(node.init, func, next, loopScope);
                    }
                    else
                    {
                        next = CompileExpression(node.init, func, next, loopScope);
                    }
                }

                next.Jump(loopCmp);

                let loopBody = CompileStatement(node.body, func, loopEntry, loopScope);
                loopBody.Jump(loopCmp);

                if (node.test)
                {
                    loopCmp = CompileExpression(node.test, func, loopCmp, loopScope);
                    let $cond = loopCmp.LastInst();
                    loopCmp.Branch($cond, loopUpdate, loopBreak);
                }
                else
                {
                    loopCmp.Jump(loopUpdate);
                }

                if (node.update)
                {
                    loopUpdate = CompileExpression(node.update, func, loopUpdate, loopScope);
                }
                loopUpdate.Jump(loopEntry);

                CompletePushScope(scope, loopScope, last, $pushScope);

                loopBreak.PopScope();
                return loopBreak;
            }
        case 'ForInStatement':
            {
            }
        case 'ForOfStatement':
        case 'FunctionDeclaration':
            return last;
        case 'IfStatement':
            {
                let consequentBlock = func.NewBlock();
                let followingBlock = func.NewBlock();
                let alternateBlock = node.alternate ? func.NewBlock() : followingBlock;

                last = CompileExpression(node.test, func, last, scope);
                let $cond = last.LastInst();
                last.Branch($cond, consequentBlock, alternateBlock);

                consequentBlock = CompileStatement(node.consequent, func, consequentBlock, scope);
                consequentBlock.Jump(followingBlock);

                if (node.alternate)
                {
                    alternateBlock = CompileStatement(node.alternate, func, alternateBlock, scope);
                    alternateBlock.Jump(followingBlock);
                }

                return followingBlock;
            }
        case 'ReturnStatement':
            {
                let $retVal;
                if (node.argument)
                {
                    last = CompileExpression(node.argument, func, last, scope);
                    $retVal = last.LastInst();
                }
                else
                {
                    $retVal = last.Undefined();
                }

                let funcScope = scope;
                let count = 0;
                while (funcScope !== null && !funcScope.func)
                {
                    funcScope = funcScope.upper;
                    count++;
                }

                if (count > 0)
                {
                    last.PopScope(count);
                }
                last.Return($retVal);

                return last;
            }
        case 'VariableDeclaration':
            {
                if (node.kind !== 'let')
                {
                    throw Error(`Declaration kind ${node.kind} is not supported`);
                }

                for (let decl of node.declarations)
                {
                    if (decl.type !== 'VariableDeclarator')
                    {
                        throw Error(`Declaration type ${decl.type} is not supported`);
                    }

                    if (decl.id.type !== 'Identifier')
                    {
                        throw Error(`Declaration ID type ${decl.id.type} is not supported`);
                    }

                    scope.Declare(decl.id.name);
                }

                for (let decl of node.declarations)
                {
                    if (decl.init)
                    {
                        let result = scope.Lookup(decl.id.name)
                        if (result.type !== 'direct')
                        {
                            throw Error('Internal');
                        }

                        last = CompileExpression(decl.init, func, last, scope);
                        last.Store(result.inst, last.LastInst());
                    }
                }

                break;
            }
        case 'WhileStatement':
            {
                let loopEntry = func.NewBlock();
                let loopCmp = func.NewBlock();
                let loopBreak = func.NewBlock();

                let loopScope = new Scope(scope, loopBreak, loopCmp);
                let {last, next} = PreparePushScope();
                let $pushScope = next.PushScope(0);
                next.Jump(loopCmp);

                let loopBody = CompileStatement(node.body, func, loopEntry, loopScope);
                loopBody.Jump(loopCmp);

                loopCmp = CompileExpression(node.test, func, loopCmp, loopScope);
                let $cond = loopCmp.LastInst();
                loopCmp.Branch($cond, loopEntry, loopBreak);

                CompletePushScope(scope, loopScope, last, $pushScope);

                loopBreak.PopScope();
                return loopBreak;
            }

        case 'SwitchStatement':
        case 'ThrowStatement':
        case 'TryStatement':
        case 'LabeledStatement':
        case 'WithStatement':
            throw Error('Not implemented');
    }

    return last;
}

function CompileNodeList(nodeList, func, scope)
{
    let entry = func.NewBlock();
    let last = entry;

    function DeclareIdentifier(id) {
        let $var = entry.Alloca(id.name);
        scope.ForwardDeclare(id.name, $var);
    }

    function ForwardDeclare(declarator)
    {
        function DeclareArrayPattern(declarator) {
            for (let ele of declarator.elements) {
                if (ele === null) {
                    continue;
                }
                else if (ele.type === 'AssignmentPattern') {
                    DeclareAssignmentPattern(ele);
                }
                else if (ele.type === 'Identifier') {
                    DeclareIdentifier(ele);
                }
                else if (ele.type === 'ArrayPattern') {
                    DeclareArrayPattern(ele);
                }
                else if (ele.type === 'ObjectPattern') {
                    DeclareObjectPattern(ele);
                }
                else if (ele.type === 'RestElement') {
                    if (ele.argument.type === 'Identifier') {
                        DeclareIdentifier(ele.argument);
                    }
                    else if (ele.argument.type === 'ArrayPattern') {
                        DeclareArrayPattern(ele.argument);
                    }
                    else if (ele.argument.type === 'ObjectPattern') {
                        DeclareObjectPattern(ele.argument);
                    }
                }
            }
        }

        function DeclareObjectPattern(declarator) {
            for (let prop of declarator.properties) {
                if (prop.key.type === 'Identifier') {
                    DeclareIdentifier(prop.key);
                }
            }
        }

        function DeclareAssignmentPattern(declarator) {
            if (declarator.left.type === 'Identifier') {
                DeclareIdentifier(declarator.left);
            }
            else if (declarator.left.type === 'ArrayPattern') {
                DeclareArrayPattern(declarator.left);
            }
            else if (declarator.left.type === 'ObjectPattern') {
                DeclareObjectPattern(declarator.left);
            }
        }

        if (declarator.id.type === 'Identifier') {
            DeclareIdentifier(declarator.id);
        }
        else if (declarator.id.type === 'ArrayPattern') {
            DeclareArrayPattern(declarator.id);
        }
        else if (declarator.id.type === 'ObjectPattern') {
            DeclareObjectPattern(declarator.id);
        }
        else {
            throw SyntaxError(`Unknown declarator type '${declarator.id.type}'`);
        }
    }

    function ForwardDeclarationInBlock(nodeList)
    {
        for (let node of nodeList)
        {
            if (node.type === 'VariableDeclaration')
            {
                if (node.kind === 'var')
                {
                    throw SyntaxError('\'var\' is no longer supported');
                }

                node.declarations.forEach(ForwardDeclare);
            }
            else if (node.type === 'ClassDeclaration')
            {
                if (node.id === null)
                {
                    // will never be referenced
                    continue;
                }

                DeclareIdentifier(node.id);
            }
            else if (node.type === 'FunctionDeclaration')
            {
                if (node.id === null)
                {
                    // will never be referenced
                    continue;
                }

                DeclareIdentifier(node.id);
            }
        }
    }

    ForwardDeclarationInBlock(nodeList);

    for (let node of nodeList)
    {
        if (node.type === 'FunctionDeclaration')
        {
            if (!node.id)
            {
                continue;
            }

            let ret = CompileFunction(node, func.ir, scope);
            let $func = last.Func(ret.func, ret.captured);

            scope.Declare(node.id.name);
            let result = scope.Lookup(node.id.name);
            if (result.type !== 'direct')
            {
                throw Error('Internal');
            }

            last.Store(result.inst, $func);
        }
    }

    for (let node of nodeList)
    {
        last = CompileStatement(node, func, last, scope);
    }

    return {
        entry,
        last
    };
}

function CompileFunction(node, ir, upperScope)
{
    let name;
    if (node.id === null)
    {
        name = '<anonymous>';
    }
    else
    {
        name = node.id.name;
    }
    let func = ir.NewFunc(name, node.params.length);

    let scope = new Scope(upperScope, null, null, func);
    let args = node.params.reduce((prev, current, index) => {
        if (current.type !== 'Identifier')
        {
            throw Error('Non-identifier argument is not supported');
        }
        prev[current.name] = index;
        return prev;
    }, {});

    scope.SetArguments(args, null);

    let first = func.NewBlock();
    let $arguments = first.Alloca('arguments');
    first.Store($arguments, first.Arguments());
    scope.Declare('arguments', $arguments);

    let {entry, last} = CompileNodeList(node.body.body, func, scope);

    first.Jump(entry);
    let $undefined = last.Undefined();
    last.Return($undefined);

    let captured = scope.CaptureList();
    return { func, captured };
}

function CompileArrowFunction(node, ir, upperScope)
{
    let func = ir.NewFunc('<arrow>', node.params.length);
    let scope = new Scope(upperScope);
    let args = node.params.reduce((prev, current, index) => {
        if (current.type !== 'Identifier')
        {
            throw Error('Non-identifier argument is not supported');
        }
        prev[current.name] = index;
    }, {});
    scope.SetArguments(args, null);
    let first = func.NewBlock();
    let $arguments = first.Alloca('arguments');
    first.Store($arguments, first.Arguments());
    scope.Declare('arguments', $arguments);

    if (node.body.type === 'BlockStatement')
    {
        let {entry, last} = CompileNodeList(node.body.body, func, scope);
        first.Jump(entry);
        let $undefined = last.Undefined();
        last.Return($undefined);
    }
    else if (node.body.type)
    {
        first = CompileExpression(node.body, func, first, scope);
        first.Return(first.LastInst());
    }

    let captured = scope.CaptureList();
    return { func, captured };

}

/**
 * @param {Program} node
 * @param {IRBuilder} ir
 * @return {IRBuilder}
 */
function CompileScript(node, ir)
{
    let rootScope = new Scope(null);

    let func = ir.NewFunc('#main', 0);
    let {entry, last} = CompileNodeList(node.body, func, rootScope);

    let $undefined = last.Undefined();
    last.Return($undefined);
}

/**
 * @param {Program} ast
 * @param {string} source
 * @return {IRBuilder}
 */
function Compile(ast, source)
{
    let ir = new IRBuilder(source);

    CompileScript(ast, ir);

    return ir;
}

module.exports = Compile;