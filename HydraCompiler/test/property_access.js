'use strict';

let t = { a : 'A' };
let i = 0;
let b;

while (i++ < 1000000)
{
    b = t.a;
}

__write(i);
