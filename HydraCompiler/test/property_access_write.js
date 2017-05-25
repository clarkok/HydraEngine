'use strict';

let t = { a : 'A' };
let i = 0;
let b = 1;

while (i++ < 1000000)
{
    t.a = b;
}

__write(i);
