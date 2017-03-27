'use strict';

function testVarInBlock()
{
    t = 0;
    console.log(t);

    {
        var t = 1;
    }

    console.log(t);
}

function testVarInLoop()
{
    t = 0;
    console.log(t);

    for (let i = 0; i < 10; ++i)
    {
        var t = i;
        console.log(t);
    }

    console.log(t);
}

function testMultipleVar()
{
    var t = 0;
    var t = 1;

    console.log(t);
}

function testFuncAfterVar()
{
    var t;

    function t () { console.log(0) }
    function t () { console.log(1) }

    console.log(t);
}

function testVarInBlockAfterLet()
{
    let t = 0;

    {
        // var t = 0;
    }
}

function testVarAfterLet()
{
    let t = 0;
    // var t = 0;
}

function testAssignmentPatternInArrayPattern()
{
    var [[a] = [1]] = [[1]];
}

function testLetInBlock()
{
    var t = 0;
    console.log(t);

    {
        let t = 1;
    }

    console.log(t);
}

function testLetInBlockAfterFunc()
{
    function t()
    {
        console.log(0);
    }

    {
        let t = 1;
    }
}

function testFuncInBlockAfterLet()
{
    let t = 0;

    {
        function t()
        {
            console.log(0);
        }
    }
}

function testFuncReferenceLet()
{
    fun();

    let ref = undefined;

    function fun()
    {
        console.log(ref);
    }

    fun();
}

testVarInBlock();
testVarInLoop();
testMultipleVar();
testFuncAfterVar();
testVarInBlockAfterLet();
testVarAfterLet();
testLetInBlock();
testLetInBlockAfterFunc();
testFuncInBlockAfterLet();
testFuncReferenceLet();
