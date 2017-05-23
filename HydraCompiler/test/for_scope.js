
let t = [];

for (let i = 0; i < 10; )
{
    t[i] = () => i;
    ++i;
    break;
}
