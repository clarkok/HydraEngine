'use strict';

if (Array.prototype.push)
{
    __write('Array.prototype.push is ready');
}
else
{
    __write('Array.prototype.push is not ready');
}

let t = [];

for (let i = 0; i < 10; ++i)
{
    t.push(i);
}

for (let i = 0; i < 10; ++i)
{
    __write(t[i]);
}
