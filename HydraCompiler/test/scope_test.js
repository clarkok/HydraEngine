'use strict';

let funcList1 = [];
let funcList2 = [];

for (let i = 0; i < 10; ++i)
{
    funcList1.push(() =>
    {
        ++i;
    });

    funcList2.push(() =>
    {
        console.log(i);
    });
}

for (let f of funcList1)
{
    f();
}

for (let f of funcList2)
{
    f();
}

function test()
{
    function t()
    {
        console.log(haha);
    }

    t();

    let haha = 1;
    t();
}

test();
