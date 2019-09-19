#include <llvm/ExecutionEngine/MCJIT.h>

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/SourceMgr.h>

#include <llvm/IR/Value.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/MDBuilder.h>

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/IPO.h>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/CodeGen/TargetLoweringObjectFileImpl.h>

#include <llvm/Linker/Linker.h>

typedef int (*func_t)();
