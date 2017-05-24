'use strict';

let t = [];

for (let i = 0; i < 10; ++i)
{
    __write(i);
    t[i] = () => i;
}

for (let i = 0; i < 10; ++i)
{
    __write(t[i]());
}
