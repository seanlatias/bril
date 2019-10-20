#include "codegen.h"

using namespace std;

CodeGen::CodeGen(IRBuilder* builder_) {
  builder = builder_;
  ctx = &(builder->getContext());
  llvm::Type* t_char_ = llvm::Type::getInt8Ty(*ctx);
  llvm::Type* t_char_p_ = llvm::Type::getInt8Ty(*ctx)->getPointerTo();
  llvm::Type* t_void_ = llvm::Type::getVoidTy(*ctx);
  llvm::Type* t_int_ = llvm::Type::getInt64Ty(*ctx);
  llvm::Type* t_bool_ = llvm::Type::getInt1Ty(*ctx);
}

llvm::Value* CodeGen::getValue(string name) {
  if (val_map.count(name)) {
    return val_map[name];
  } else {
    llvm::Value* alloca_val = builder->CreateAlloca(t_bool_, llvm::ConstantInt::getSigned(t_int_, 1));
    val_map[name] = alloca_val;
    return alloca_val;
  }
}

#define ADD_BIN_OP(OP) \
  assert(json_dest && "##OPinstruction missing field dest!\n"); \
  string dest = json_dest.getValue().str(); \
  llvm::Value* lhs_ptr = getValue(args[0]); \
  llvm::Value* rhs_ptr = getValue(args[1]); \
  llvm::Value* lhs = builder->CreateLoad(lhs_ptr); \
  llvm::Value* rhs = builder->CreateLoad(rhs_ptr); \
  llvm::Value* val = builder->Create##OP(lhs, rhs, dest); \
  builder->CreateStore(val, getValue(dest));

void CodeGen::createInst(llvm::json::Object* obj, BasicBlockMap_T* bb_map) {
  // LLVM does not allow a basic block to end with a non-terminate instruction
  // in our case, the basic block must end with either jump, branch, or return
  // since in Bril, all basic blocks other than the entry block must have a label
  // bril instrucitons immediately following a jump, branch or return are not useful at all
  // (there is an example in createFunction() )
  // here we check the terminating instruction of the block
  // if it exists, then we don't insert instruction into this block any more
  llvm::BasicBlock* bb = builder->GetInsertBlock();
  llvm::LLVMContext* ctx = &(builder->getContext());
  llvm::Module* module = bb->getModule();
  if (llvm::Instruction* term_inst = bb->getTerminator())
    return;

  // read common keys
  auto json_op = obj->getString("op");
  // all instructions must have this
  assert(json_op && "Instruction missing field op!\n");

  auto json_dest = obj->getString("dest");
  auto json_type = obj->getString("type");
  auto json_args = obj->getArray("args");
  // put the arguments into the vectors
  VarList_T args;
  args.clear();
  if (json_args) {
    for (auto ai = json_args->begin(), ae = json_args->end(); ai != ae; ai ++ ) {
      if (auto json_str = ai->getAsString()) {
        string arg_str = json_str.getValue().str();
        args.push_back(arg_str);
      }
      else 
        assert(false && "Reading argument error!\n");
    }
  }

  // we simply allocate one memory location for each bril variable
  // when the variable is used, we load from that location
  // when the variable is modified, we store to that location
  // this is similar with what clang does when not running any optimization
  // the mapping from variable names to pointers (LLVM Value*) is stored in val_map
  string op = json_op.getValue().str();

  if (op == "add") { 
    ADD_BIN_OP(Add)
  } 
  else if (op == "mul") {
    ADD_BIN_OP(Mul)
  } 
  else if (op == "sub") {
    ADD_BIN_OP(Sub)
  } 
  else if (op == "div") {
    ADD_BIN_OP(SDiv)
  } 
  else if (op == "eq") {
    ADD_BIN_OP(ICmpEQ)
  }
  else if (op == "lt") {
    ADD_BIN_OP(ICmpSLT)
  }
  else if (op == "gt") {
    ADD_BIN_OP(ICmpSGT)
  }
  else if (op == "and") {
    ADD_BIN_OP(And)
  }
  else if (op == "or") {
    ADD_BIN_OP(Or)
  }
  else if (op == "not") {
    assert(json_dest && "not instruction missing field dest!\n");
    string dest = json_dest.getValue().str();
    llvm::Value* val_ptr = getValue(args[0]);
    llvm::Value* val = builder->CreateLoad(val_ptr);
    llvm::Value* not_val = builder->CreateNot(val, dest);
    builder->CreateStore(not_val, getValue(dest));
  }
  else if (op == "const") {
    // get all the useful fields
    assert(json_dest && "const instruction missing field dest!\n");
    string dest = json_dest.getValue().str();
    assert(json_type && "const instructon missing field type!\n");
    string type = json_type.getValue().str();
    int int_val;
    bool bool_val;
    if (type == "int") {
      int_val = (obj->getInteger("value")).getValue();
      llvm::Value* alloca_val = builder->CreateAlloca(t_int_, llvm::ConstantInt::getSigned(t_int_, 1));
      val_map[dest] = alloca_val;
      builder->CreateStore(llvm::ConstantInt::get(t_int_, int_val, true), alloca_val);
    }
    else if (type == "bool") {
      bool_val = (obj->getBoolean("value")).getValue();
      llvm::Value* alloca_val = builder->CreateAlloca(t_bool_, llvm::ConstantInt::getSigned(t_int_, 1));
      val_map[dest] = alloca_val;
      builder->CreateStore(llvm::ConstantInt::get(t_bool_, bool_val), alloca_val);
    }
    else 
      assert(false && "const instruction type does not match!\n");
  } 
  else if (op == "jmp") {
    string target = args[0];
    llvm::BasicBlock* target_bb = (*bb_map)[target].first;
    builder->CreateBr(target_bb);
    (*bb_map)[target].second = true;
  }
  else if (op == "br") {
    llvm::Value* cond_ptr = getValue(args[0]);
    string true_target = args[1];
    string false_target = args[2];
    llvm::BasicBlock* true_target_bb = (*bb_map)[true_target].first;
    llvm::BasicBlock* false_target_bb = (*bb_map)[false_target].first;
    llvm::Value* cond = builder->CreateLoad(cond_ptr);
    builder->CreateCondBr(cond, true_target_bb, false_target_bb);
    (*bb_map)[true_target].second = true;
    (*bb_map)[false_target].second = true;
  }
  else if (op == "ret") {
    builder->CreateRet(llvm::ConstantInt::get(t_int_, 0, true));
  }
  else if (op == "id") {
    // no example code for this instruction
    // assume that the destination is stored in "dest", the argument stored in "args"
    assert(json_dest && "id instruction missing field dest!\n");
    string dest = json_dest.getValue().str();
    llvm::Value* val_ptr = getValue(args[0]);
    llvm::Value* val = builder->CreateLoad(val_ptr);
    builder->CreateStore(val, getValue(dest));
  }
  else if (op == "print") {
    llvm::Value* val_ptr = getValue(args[0]);
    llvm::Value* val = builder->CreateLoad(val_ptr);
    std::vector<llvm::Type*> call_types;
    call_types.push_back(t_char_p_);
    call_types.push_back(val->getType());
    llvm::FunctionType* call_ftype = llvm::FunctionType::get(t_int_, call_types, false);
    llvm::Function* printf_call = llvm::cast<llvm::Function>(module->getOrInsertFunction("printf", call_ftype));
    std::vector<llvm::Value*> printf_args;
    printf_args.push_back(builder->CreateGlobalStringPtr("%d\n"));
    printf_args.push_back(val);
    builder->CreateCall(printf_call, printf_args);
  }
  // do nothing for nop
  else if (op == "nop") ; 
  else 
    assert(false && "Operation not supported!\n");
}

