add_library(HydraCore.VirtualMachine OBJECT
    CompiledFunction.cpp
    CompiledFunction.h
	IR.cpp
	IR.h
	IRInsts.cpp
	IRInsts.h
    ByteCode.cpp
    ByteCode.h
    Replacable.h
    Scope.cpp
    Scope.h
    VM.cpp
    VMDefs.h
)
