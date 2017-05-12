'use strict';

let StringPool = require('./string-pool');
let BufferWriter = require('./buffer-writer.js');
let fs = require('fs');

class ByteCode
{
    constructor()
    {
        this.sections = [];
    }

    AddSection(writer, type)
    {
        this.sections.push({
            type,
            writer,
        });
    }

    Store(filename)
    {
        let fd = fs.openSync(filename, 'w');
        let fileOffset = 0;

        let WriteBufferWriter = (writer) =>
        {
            let lengthBuffer = Buffer.alloc(4);
            lengthBuffer.writeUInt32LE(writer.length);

            fs.writeSync(fd, lengthBuffer, 0, 4, fileOffset);
            fileOffset += 4;

            fs.writeSync(fd, writer.buffer, 0, writer.length, fileOffset);
            fileOffset += writer.length;

            fileOffset = (fileOffset + 7) & -8;
        };

        let WriteHeader = () =>
        {
            let writer = new BufferWriter();
            writer.byte(72);    // H
            writer.byte(89);    // Y
            writer.byte(73);    // I
            writer.byte(82);    // R

            writer.uint(this.sections.length);

            let offset = 12 + (8 * this.sections.length);
            offset = (offset + 7) & -8;
            for (let section of this.sections)
            {
                writer.uint(section.type)
                writer.uint(offset);

                offset += section.writer.length + 4;
                offset = (offset + 7) & -8;
            }

            WriteBufferWriter(writer);
        };

        WriteHeader();
        for (let section of this.sections)
        {
            WriteBufferWriter(section.writer);
        }
    }

    static get SECTION_TYPE()
    {
        return {
            FUNCTION : 0,
            STRING_POOL : 1,
        }
    }
}

module.exports = ByteCode;