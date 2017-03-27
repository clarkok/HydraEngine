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
    }

    /**
     * @param {string} name 
     * @returns {{index : number, type : 'direct'|'capture'}}
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

    Declare(name, inst)
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
        case 'ObjectExpression':
        case 'FunctionExpression':
        case 'ArrowFunctionExpression':
        case 'ClassExpression':
        case 'TaggedExpression':
        case 'MemberExpression':
        case 'Super':
        case 'MetaProperty':
        case 'NewExpression':
        case 'CallExpression':
        case 'UpdateExpression':
        case 'AwaitExpression':
        case 'UnaryExpression':
        case 'BinaryExpression':
        case 'LogicalExpression':
        case 'ConditionalExpression':
        case 'YieldExpression':
        case 'AssignmentExpression':
        case 'SequenceExpression':
            throw Error('Not implemented');
    }
    return last;
}

function CompileExpressionReference(node, func, last, scope)
{
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
            let localSymbol = scope.Lookup(name, true);
            if (localSymbol.type === 'direct')
            {
                $pushScope.capture.push(localSymbol.inst);
            }
            else if (localSymbol.type === 'capture')
            {
                let $cap = last.Capture(localSymbol.index);
                $pushScope.capture.push($cap);
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
            {
                throw Error('Not implemented');
            }
            break;
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
                    if (node.type === 'VariableDeclaration')
                    {
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

                last.PopScope(count);
                last.Return($retVal);

                return last;
            }
        case 'VariableDeclaration':
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

    function ForwardDeclare(declarator)
    {
        function DeclareIdentifier(declarator)
        {
            let $var = entry.Alloca();
            scope.ForwardDeclare(declarator.id.name, $var);
        }

        function DeclareArrayPattern(declarator)
        {
            for (let ele of declarator.elements)
            {
                if (ele === null)
                {
                    continue;
                }
                else if (ele.type === 'AssignmentPattern')
                {
                    DeclareAssignmentPattern(ele);
                }
                else if (ele.type === 'Identifier')
                {
                    DeclareIdentifier(ele);
                }
                else if (ele.type === 'ArrayPattern')
                {
                    DeclareArrayPattern(ele);
                }
                else if (ele.type === 'ObjectPattern')
                {
                    DeclareObjectPattern(ele);
                }
                else if (ele.type === 'RestElement')
                {
                    if (ele.argument.type === 'Identifier')
                    {
                        DeclareIdentifier(ele.argument);
                    }
                    else if (ele.argument.type === 'ArrayPattern')
                    {
                        DeclareArrayPattern(ele.argument);
                    }
                    else if (ele.argument.type === 'ObjectPattern')
                    {
                        DeclareObjectPattern(ele.argument);
                    }
                }
            }
        }

        function DeclareObjectPattern(declarator)
        {
            for (let prop of declarator.properties)
            {
                if (prop.key.type === 'Identifier')
                {
                    DeclareIdentifier(prop.key);
                }
            }
        }

        function DeclareAssignmentPattern(declarator)
        {
            if (declarator.left.type === 'Identifier')
            {
                DeclareIdentifier(declarator.left);
            }
            else if (declarator.left.type === 'ArrayPattern')
            {
                DeclareArrayPattern(declarator.left);
            }
            else if (declarator.left.type === 'ObjectPattern')
            {
                DeclareObjectPattern(declarator.left);
            }
        }

        if (declarator.id.type === 'Identifier')
        {
            DeclareIdentifier(declarator);
        }
        else if (declarator.id.type === 'ArrayPattern')
        {
            DeclareArrayPattern(declarator);
        }
        else if (declarator.id.type === 'ObjectPattern')
        {
            DeclareObjectPattern(declarator);
        }
        else
        {
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
        last = CompileStatement(node, func, last, scope);
    }

    return {
        entry,
        last
    };
}

function CompileFunction(node, ir, upperScope)
{
}

/**
 * @param {Program} node
 * @param {IRBuilder} ir
 * @return {IRBuilder}
 */
function CompileScript(node, ir)
{
    let rootScope = new Scope(null);

    let func = ir.NewFunc('#main');
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