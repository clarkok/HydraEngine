function module(init)
{
    let __modules = global.__modules;
    let name = this.name;

    let moduleDesc = __modules[name];
    if (!moduleDesc)
    {
        moduleDesc = {
            name : this.name,
            dirname : this.dirname,
            exports : undefined
        };
    }

    moduleDesc.exports = init(moduleDesc);

    __modules[name] = moduleDesc;

    return moduleDesc.exports;
}

function include(path)
{
    let normalized = __normalize_path(this.dirname, path);

    let __modules = global.__modules;

    if (!__modules[normalized])
    {
        __modules[normalized] = {
            name : normalized,
            exports : undefined
        };
        __execute(normalized);
    }

    return __modules[normalized].exports;
}

global.__modules = {};
global.module = module;
global.include = include;

this.include('./array.ir');

