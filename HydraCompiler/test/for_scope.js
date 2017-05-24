
let t = [];

for (let i = 0; i < 10; )
{
    t[i] = () => i;
    ++i;
    break;
}

for (let i = 0; i < t.length; ++i)
{
    __write(t[i]());
}
