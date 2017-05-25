Array.prototype.push = function ()
{
    let length = this.length | 0;
    let i = 0;
    let argLength = arguments.length | 0;
    while (i < argLength)
    {
        this[length++] = arguments[i++];
    }

    if (Array.isArray(this))
    {
        return this.length;
    }
    return this.length = length;
};
