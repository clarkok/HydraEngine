'use strict';

let TEST_KEY = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz01234568789';

let count = 0;

let id = 0;
class TestHeapObject
{
    constructor(Ref1 = null, Ref2 = null, Ref3 = null)
    {
        this.id = id++;
        this.Ref1 = Ref1;
        this.Ref2 = Ref2;
        this.Ref3 = Ref3;
    }
};

let uut = {};
let values = new Array(40);

while (true)
{
    if (count++ % 8192 == 0)
    {
        console.log(Date.now());
    }

    for (let i = 0; i < 40; ++i)
    {
        let key = TEST_KEY.substr(i, 1);
        let value = new TestHeapObject();

        uut[key] = value;
        values[i] = value;
    }

    for (let i = 0; i < 40; ++i)
    {
        let key = TEST_KEY.substr(i, 1);
        let value = null;

        value = uut[key]
        if (value !== values[i])
        {
            throw new Error();
        }
    }
}
