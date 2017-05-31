let N = 100000;
let concurrency = 4;

let arr = [];

function sort(arr, start, end)
{
    if (end <= start + 1)
    {
        return;
    }

    if (end - start < 32)
    {
        for (let i = start; i < end; ++i)
        {
            for (let j = i + 1; j < end; ++j)
            {
                if (arr[i] > arr[j])
                {
                    let t = arr[i];
                    arr[i] = arr[j];
                    arr[j] = t;
                }
            }
        }
        return;
    }

    let pivot = arr[end - 1];
    let i = start;
    for (let j = start; j < end - 1; ++j)
    {
        if (arr[j] <= pivot)
        {
            {
                let t = arr[i];
                arr[i] = arr[j];
                arr[j] = t;
            }
            ++i;
        }
    }
    {
        let t = arr[i];
        arr[i] = arr[end - 1];
        arr[end - 1] = t;
    }

    sort(arr, start, i);
    sort(arr, i + 1, end);
}

function merge(arr, sliced)
{
    let index = [];
    for (let i = 0; i < sliced.length; ++i)
    {
        index[i] = 0;
    }

    for (let i = 0; i < arr.length; ++i)
    {
        let min = 0;
        for (let j = 1; j < sliced.length; ++j)
        {
            if (index[j] >= sliced[j].length)
            {
                continue;
            }

            if (sliced[j][index[j]] < sliced[min][index[min]])
            {
                min = j;
            }
        }

        arr[i] = sliced[min][index[min]++];
    }
}

function sort_s(arr)
{
    console.log('sort');

    let sliced = [];
    let length = arr.length;

    for (let i = 0; i < concurrency; ++i)
    {
        let start = (length * i / concurrency) | 0;
        let end = (length * (i + 1) / concurrency) | 0;
        console.log('Part ', i, ' from ', start, ' to ', end);

        let s = arr.slice(start, end);
        sort(s, 0, s.length);
        sliced[i] = s;
    }

    console.log('merge');
    merge(arr, sliced);
}

for (let i = 0; i < N; ++i)
{
    arr[i] = (Math.random() * N) | 0;
    if (i % 1000 == 0)
    {
        console.log(i);
    }
}

let start = Date.now();
sort_s(arr);
console.log(Date.now() - start, "ms");
