#ifndef CODEGEN_H
#define CODEGEN_H

#include <iostream>
#include "common.h"

using namespace std;

typedef pair<llvm::BasicBlock*, bool> BasicBlockFlag_T;
typedef map<string, BasicBlockFlag_T> BasicBlockMap_T;
typedef map<string, llvm::Value*>     VarToVal_T;
typedef vector<string>                VarList_T;
typedef int (*MainFunc_T)();

using IRBuilder = llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>;

class CodeGen {
 public:
  CodeGen(IRBuilder* builder);
  // create an instruction from the json object, place it into a basic block using IRBuilder
  void createInst(llvm::json::Object* obj, BasicBlockMap_T* bb_map);

 private:
  // useful types
  llvm::Type* t_char_;
  llvm::Type* t_char_p_;
  llvm::Type* t_void_;
  llvm::Type* t_int_;
  llvm::Type* t_bool_;

  // IRBuilder
  IRBuilder* builder;
  llvm::LLVMContext* ctx;

  // containers
  VarToVal_T val_map;

  // get llvm value from name
  llvm::Value* getValue(string name);
};

#endif
