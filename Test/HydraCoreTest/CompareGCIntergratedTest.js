'use strict';

let id = 0;
class TestHeapObject
{
    constructor(Ref1 = null, Ref2 = null, Ref3 = null)
    {
        this.Ref1 = Ref1;
        this.Ref2 = Ref2;
        this.Ref3 = Ref3;
        this.Id = id++;
    }
};

let round = 0;
let headers = new Array(10);

while (true)
{
    if (round++ % 8192 == 0)
    {
        console.log(Date.now());
    }

    let head = null;
    let count = 1000;

    while (count--)
    {
        head = new TestHeapObject(head);
    }

    let ptr = head;
    let lastId = ptr.Id;
    count = 1
    while (ptr.Ref1)
    {
        ptr = ptr.Ref1;
        if (ptr.Id != lastId - 1)
        {
            throw new Error();
        }
        lastId = ptr.Id;
        count++;
    }

    headers[round % 10] = head;
    if (count != 1000)
    {
        throw new Error();
    }
}
