'use strict';

function decl()
{
    console.log('test');
}

function WithArgs(a, b)
{
    console.log(b);
}

function Capture()
{
    WithArgs(1, 2);
}

function CaptureArguments(t)
{
    return function () {
        return t;
    }
}

function ReferringToArguments()
{
    return arguments;
}
