let N = 100000;
let concurrency = 4;

let arr = [];

function rand()
{
    return (__random() * N) | 0;
}

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
    let slicedLength = sliced.length;
    for (let i = 0; i < slicedLength; ++i)
    {
        index[i] = 0;
    }

    let arrayLength = arr.length;
    for (let i = 0; i < arrayLength; ++i)
    {
        let min = 0;
        for (let j = 1; j < slicedLength; ++j)
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

function sort_p(arr)
{
    let start = __datetime();

    __write('sort_p');

    if (concurrency == 1)
    {
        sort(arr, 0, arr.length);
        __write((__datetime() - start), "ms");
        return;
    }

    let tasks = [];
    let length = arr.length;

    for (let i = 0; i < concurrency; ++i)
    {
        let start = (length * i / concurrency) | 0;
        let end = (length * (i + 1) / concurrency) | 0;
        __write('Part ', i, ' from ', start, ' to ', end);

        tasks[i] = () => {
            let sliced = arr.slice(start, end);
            sort(sliced, 0, sliced.length);
            return sliced;
        }
    }

    let sliced = __parallel(tasks, concurrency);

    __write((__datetime() - start), "ms");

    __write('merge');
    merge(arr, sliced);

    __write((__datetime() - start), "ms");
}

for (let i = 0; i < N; ++i)
{
    arr[i] = rand();
    if (i % 1000 == 0)
    {
        __write(i);
    }
}

__write('sorting');

sort_p(arr);
