'use strict';

let esprima = require('esprima');
let fs = require('fs');
let Compile = require('./lib/compiler.js');

function ParseArguments()
{
    let sourceFiles = [];

    for (let i = 2; i < process.argv.length; ++i)
    {
        sourceFiles.push(process.argv[i]);
    }

    return {
        sourceFiles
    };
}

let args = ParseArguments();

for (let source of args.sourceFiles)
{
    let code = fs.readFileSync(source, 'utf8').replace(/^\uFEFF/, '');
    let ast = esprima.parse(code, { sourceType: 'module', loc : true });
    let ir = Compile(ast, source);

    console.log(ir.toString());
}