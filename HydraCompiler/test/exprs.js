let t = [1, 2, 3, 4];

let a = [0, 1, 2, ...t, 3, 4, 5];

let b = {
    a,
    b : 1,
    0 : 1,
    [1] : 2
};

new Array(10);

Array(10, 20, 30);

Array.of([1, 2, 3]);

t++;
++t;
t--;
--t;

t[0]++;
++t[0];
b.b--;
--b.b;

'logical expression';
a && b;
a || b;