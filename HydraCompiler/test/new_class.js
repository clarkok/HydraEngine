'use strict';

function Person(name, gender)
{
    this.name = name;
    this.gender = gender;
}

Person.prototype.IsMale = function () {
    return this.gender === 'Male';
};

Person.prototype.GetName = function () {
    return this.name;
};

let t = new Person('Clarkok', 'Male');

__write(t.IsMale());
__write(t.GetName());
