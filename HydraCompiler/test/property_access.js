'use strict';

let t = {};
let i = 0;

while (i++ < 100000)
{
    t.a = 'A';
}

__write(i);
