'use strict';

let t = [];

for (let i = 0; i < 10; ++i)
{
    t.push(() => i);
}
