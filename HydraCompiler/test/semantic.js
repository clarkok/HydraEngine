'use strict';

function assert(cond, message)
{
    if (!cond)
    {
        __write(message);
    }
}

assert(1 == 1,              '1 == 1');
assert('1' == 1,            '\'1\' == 1');
assert(1 == '1',            '1 == \'1\'');
assert(0 == false,          '0 == false');
assert(0 != null,           '0 != null');
assert(0 != undefined,      '0 != undefined');
assert(null == undefined,   'null == undefined');

