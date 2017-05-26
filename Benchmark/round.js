let N = 100000;
let K = 5;

function runRound()
{
    let result = [];
    let queue = null;

    for (let i = 0; i < N; ++i)
    {
        let node = new Node(null, i);
        queue = node.AddTo(queue);
    }

    while (queue)
    {
        for (let i = 0; i < K - 1; ++i)
        {
            let front = queue;
            queue = queue.queue;

            queue = front.AddTo(queue);
        }

        let front = queue;
        queue = queue.queue;

        result.push(front.value);
    }

    /*
    __write(result.length);
    for (let i = 0; i < N; ++i)
    {
        __write("  " + result[i]);
    }
    */

    if (result.length !== N)
    {
        __write("result length not match: ", result.length);
    }
}

function Node(queue, value)
{
    this.queue = queue;
    this.value = value;
}

Node.prototype.AddTo = function (queue)
{
    if (queue === null)
    {
        return this;
    }

    let t = queue;
    while (t.queue !== null)
    {
        t = t.queue;
    }
    t.queue = this;
    this.queue = null;
    return queue;
}

runRound();
