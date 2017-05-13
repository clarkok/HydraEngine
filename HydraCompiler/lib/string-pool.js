'use strict';

let BufferWriter = require('./buffer-writer');

class StringPool
{
    constructor()
    {
        this.writer = new BufferWriter();
        this.map = {};
    }

    Get(string)
    {
        if (this.map.hasOwnProperty(string))
        {
            return this.map[string];
        }
        return this.map[string] = this.writer.string(string);
    }
}

module.exports = StringPool;