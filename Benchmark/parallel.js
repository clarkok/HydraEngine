let tasks = [];

for (let i = 0; i < 10; ++i)
{
    tasks.push(() => {
        __write(i);
        return i;
    });
}

let result = __parallel(tasks);
for (let i = 0; i < 10; ++i)
{
    __write(result[i]);
}
