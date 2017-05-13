'use strict';

const BLOCK_SIZE = 4096;

class BufferWriter
{
    constructor()
    {
        this.buffer = Buffer.alloc(BLOCK_SIZE);
        this.capacity = BLOCK_SIZE;
        this.length = 0;
    }

    byte(value)
    {
        this.CheckCapacity(1);
        let ret = this.length;
        this.length = this.buffer.writeUInt8(value, this.length);
        return ret;
    }

    uint(number)
    {
        this.CheckCapacity(4);
        let ret = this.length;
        this.length = this.buffer.writeUInt32LE(number, this.length);
        return ret;
    }

    double(number)
    {
        this.CheckCapacity(8);
        let ret = this.length;
        this.length = this.buffer.writeDoubleLE(number, this.length);
        return ret;
    }

    string(string)
    {
        this.Align(4);
        let ret = this.uint(string.length);
        this.CheckCapacity(string.length * 2);
        this.length += this.buffer.write(string, this.length, string.length * 2, 'utf16le');
        return ret;
    }

    CheckCapacity(size)
    {
        if (this.length + size > this.capacity)
        {
            this.Enlarge();
        }
    }

    Enlarge()
    {
        let newBuffer = Buffer.alloc(this.capacity + BLOCK_SIZE);
        this.buffer.copy(newBuffer);
        this.buffer = newBuffer;
        this.capacity += BLOCK_SIZE;
    }

    Align(size)
    {
        let mod = this.length % size;
        if (mod !== 0)
        {
            this.length += (size - mod);
            this.CheckCapacity(0);
        }
    }
}

module.exports = BufferWriter;