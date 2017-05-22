let t = { a : 1 };
let u = { __proto__ : t };

__write(u.a);

let a = [1, 2]

__write(a.length);
