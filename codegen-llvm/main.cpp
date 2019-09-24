#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <utility>
#include <cassert>
#include "common.h"
#include "llvm/Support/JSON.h"

using namespace std;

using IRBuilder = llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>;
using BasicBlockFlag_T = pair<llvm::BasicBlock*, bool> ;
using BasicBlockMap_T = map<string, BasicBlockFlag_T>;
using VarToVal_T = map<string, llvm::Value*>;
using VarList_T = vector<string>;

// create an instruction from the json object, place it into a basic block using IRBuilder
void createInst(
  IRBuilder* builder,
  llvm::json::Object* obj,
  BasicBlockMap_T* bb_map,
  VarToVal_T* val_map) {

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

  // useful types
  llvm::Type* t_char_ = llvm::Type::getInt8Ty(*ctx);
  llvm::Type* t_char_p_ = llvm::Type::getInt8Ty(*ctx)->getPointerTo();
  llvm::Type* t_void_ = llvm::Type::getVoidTy(*ctx);
  llvm::Type* t_int_ = llvm::Type::getInt64Ty(*ctx);
  llvm::Type* t_bool_ = llvm::Type::getInt1Ty(*ctx);

  // we simply allocate one memory location for each bril variable
  // when the variable is used, we load from that location
  // when the variable is modified, we store to that location
  // this is similar with what clang does when not running any optimization
  // the mapping from variable names to pointers (LLVM Value*) is stored in val_map
  string op = json_op.getValue().str();
  cout << op << endl;

  if (op == "const") {
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
      (*val_map)[dest] = alloca_val;
      builder->CreateStore(llvm::ConstantInt::get(t_int_, int_val, true), alloca_val);
    }
    else if (type == "bool") {
      bool_val = (obj->getBoolean("value")).getValue();
      llvm::Value* alloca_val = builder->CreateAlloca(t_bool_, llvm::ConstantInt::getSigned(t_int_, 1));
      (*val_map)[dest] = alloca_val;
      builder->CreateStore(llvm::ConstantInt::get(t_bool_, bool_val), alloca_val);
    }
    else 
      assert(false && "const instruction type does not match!\n");
  }
  else if (op == "add") {
    assert(json_dest && "add instruction missing field dest!\n");
    string dest = json_dest.getValue().str();
    llvm::Value* lhs_ptr = (*val_map)[args[0]];
    llvm::Value* rhs_ptr = (*val_map)[args[1]];
    llvm::Value* lhs = builder->CreateLoad(lhs_ptr);
    llvm::Value* rhs = builder->CreateLoad(rhs_ptr);
    llvm::Value* add_val = builder->CreateAdd(lhs, rhs, dest);
    if (val_map->count(dest)) {
      llvm::Value* dest_ptr = (*val_map)[dest];
      builder->CreateStore(add_val, dest_ptr);
    }
    else {
      llvm::Value* alloca_val = builder->CreateAlloca(t_int_, llvm::ConstantInt::getSigned(t_int_, 1));
      (*val_map)[dest] = alloca_val;
      builder->CreateStore(add_val, alloca_val);
    }
  }
  else if (op == "mul") {
    assert(json_dest && "mul instruction missing field dest!\n");
    string dest = json_dest.getValue().str();
    llvm::Value* lhs_ptr = (*val_map)[args[0]];
    llvm::Value* rhs_ptr = (*val_map)[args[1]];
    llvm::Value* lhs = builder->CreateLoad(lhs_ptr);
    llvm::Value* rhs = builder->CreateLoad(rhs_ptr);
    llvm::Value* mul_val = builder->CreateMul(lhs, rhs, dest);
    if (val_map->count(dest)) {
      llvm::Value* dest_ptr = (*val_map)[dest];
      builder->CreateStore(mul_val, dest_ptr);
    }
    else {
      llvm::Value* alloca_val = builder->CreateAlloca(t_int_, llvm::ConstantInt::getSigned(t_int_, 1));
      (*val_map)[dest] = alloca_val;
      builder->CreateStore(mul_val, alloca_val);
    }
  }
  else if (op == "sub") {
    assert(json_dest && "sub instruction missing field dest!\n");
    string dest = json_dest.getValue().str();
    llvm::Value* lhs_ptr = (*val_map)[args[0]];
    llvm::Value* rhs_ptr = (*val_map)[args[1]];
    llvm::Value* lhs = builder->CreateLoad(lhs_ptr);
    llvm::Value* rhs = builder->CreateLoad(rhs_ptr);
    llvm::Value* sub_val = builder->CreateSub(lhs, rhs, dest);
    if (val_map->count(dest)) {
      llvm::Value* dest_ptr = (*val_map)[dest];
      builder->CreateStore(sub_val, dest_ptr);
    }
    else {
      llvm::Value* alloca_val = builder->CreateAlloca(t_int_, llvm::ConstantInt::getSigned(t_int_, 1));
      (*val_map)[dest] = alloca_val;
      builder->CreateStore(sub_val, alloca_val);
    }
  }
  else if (op == "div") {
    assert(json_dest && "div instruction missing field dest!\n");
    string dest = json_dest.getValue().str();
    llvm::Value* lhs_ptr = (*val_map)[args[0]];
    llvm::Value* rhs_ptr = (*val_map)[args[1]];
    llvm::Value* lhs = builder->CreateLoad(lhs_ptr);
    llvm::Value* rhs = builder->CreateLoad(rhs_ptr);
    llvm::Value* div_val = builder->CreateSDiv(lhs, rhs, dest);
    if (val_map->count(dest)) {
      llvm::Value* dest_ptr = (*val_map)[dest];
      builder->CreateStore(div_val, dest_ptr);
    }
    else {
      llvm::Value* alloca_val = builder->CreateAlloca(t_int_, llvm::ConstantInt::getSigned(t_int_, 1));
      (*val_map)[dest] = alloca_val;
      builder->CreateStore(div_val, alloca_val);
    }
  }
  else if (op == "eq") {
    assert(json_dest && "eq instruction missing field dest!\n");
    string dest = json_dest.getValue().str();
    llvm::Value* lhs_ptr = (*val_map)[args[0]];
    llvm::Value* rhs_ptr = (*val_map)[args[1]];
    llvm::Value* lhs = builder->CreateLoad(lhs_ptr);
    llvm::Value* rhs = builder->CreateLoad(rhs_ptr);
    llvm::Value* eq_val = builder->CreateICmpEQ(lhs, rhs, dest);
    if (val_map->count(dest)) {
      llvm::Value* dest_ptr = (*val_map)[dest];
      builder->CreateStore(eq_val, dest_ptr);
    }
    else {
      llvm::Value* alloca_val = builder->CreateAlloca(t_bool_, llvm::ConstantInt::getSigned(t_int_, 1));
      (*val_map)[dest] = alloca_val;
      builder->CreateStore(eq_val, alloca_val);
    }
  }
  else if (op == "lt") {
    assert(json_dest && "lt instruction missing field dest!\n");
    string dest = json_dest.getValue().str();
    llvm::Value* lhs_ptr = (*val_map)[args[0]];
    llvm::Value* rhs_ptr = (*val_map)[args[1]];
    llvm::Value* lhs = builder->CreateLoad(lhs_ptr);
    llvm::Value* rhs = builder->CreateLoad(rhs_ptr);
    llvm::Value* lt_val = builder->CreateICmpSLT(lhs, rhs, dest);
    if (val_map->count(dest)) {
      llvm::Value* dest_ptr = (*val_map)[dest];
      builder->CreateStore(lt_val, dest_ptr);
    }
    else {
      llvm::Value* alloca_val = builder->CreateAlloca(t_bool_, llvm::ConstantInt::getSigned(t_int_, 1));
      (*val_map)[dest] = alloca_val;
      builder->CreateStore(lt_val, alloca_val);
    }
  }
  else if (op == "gt") {
    assert(json_dest && "gt instruction missing field dest!\n");
    string dest = json_dest.getValue().str();
    llvm::Value* lhs_ptr = (*val_map)[args[0]];
    llvm::Value* rhs_ptr = (*val_map)[args[1]];
    llvm::Value* lhs = builder->CreateLoad(lhs_ptr);
    llvm::Value* rhs = builder->CreateLoad(rhs_ptr);
    llvm::Value* gt_val = builder->CreateICmpSGT(lhs, rhs, dest);
    if (val_map->count(dest)) {
      llvm::Value* dest_ptr = (*val_map)[dest];
      builder->CreateStore(gt_val, dest_ptr);
    }
    else {
      llvm::Value* alloca_val = builder->CreateAlloca(t_bool_, llvm::ConstantInt::getSigned(t_int_, 1));
      (*val_map)[dest] = alloca_val;
      builder->CreateStore(gt_val, alloca_val);
    }
  }
  else if (op == "not") {
    assert(json_dest && "not instruction missing field dest!\n");
    string dest = json_dest.getValue().str();
    llvm::Value* val_ptr = (*val_map)[args[0]];
    llvm::Value* val = builder->CreateLoad(val_ptr);
    llvm::Value* not_val = builder->CreateNot(val, dest);
    if (val_map->count(dest)) {
      llvm::Value* dest_ptr = (*val_map)[dest];
      builder->CreateStore(not_val, dest_ptr);
    }
    else {
      llvm::Value* alloca_val = builder->CreateAlloca(t_bool_, llvm::ConstantInt::getSigned(t_int_, 1));
      (*val_map)[dest] = alloca_val;
      builder->CreateStore(not_val, alloca_val);
    }
  }
  else if (op == "and") {
    assert(json_dest && "and instruction missing field dest!\n");
    string dest = json_dest.getValue().str();
    llvm::Value* lhs_ptr = (*val_map)[args[0]];
    llvm::Value* rhs_ptr = (*val_map)[args[1]];
    llvm::Value* lhs = builder->CreateLoad(lhs_ptr);
    llvm::Value* rhs = builder->CreateLoad(rhs_ptr);
    llvm::Value* and_val = builder->CreateAnd(lhs, rhs, dest);
    if (val_map->count(dest)) {
      llvm::Value* dest_ptr = (*val_map)[dest];
      builder->CreateStore(and_val, dest_ptr);
    }
    else {
      llvm::Value* alloca_val = builder->CreateAlloca(t_bool_, llvm::ConstantInt::getSigned(t_int_, 1));
      (*val_map)[dest] = alloca_val;
      builder->CreateStore(and_val, alloca_val);
    }
  }
  else if (op == "or") {
    assert(json_dest && "or instruction missing field dest!\n");
    string dest = json_dest.getValue().str();
    llvm::Value* lhs_ptr = (*val_map)[args[0]];
    llvm::Value* rhs_ptr = (*val_map)[args[1]];
    llvm::Value* lhs = builder->CreateLoad(lhs_ptr);
    llvm::Value* rhs = builder->CreateLoad(rhs_ptr);
    llvm::Value* or_val = builder->CreateOr(lhs, rhs, dest);
    if (val_map->count(dest)) {
      llvm::Value* dest_ptr = (*val_map)[dest];
      builder->CreateStore(or_val, dest_ptr);
    }
    else {
      llvm::Value* alloca_val = builder->CreateAlloca(t_bool_, llvm::ConstantInt::getSigned(t_int_, 1));
      (*val_map)[dest] = alloca_val;
      builder->CreateStore(or_val, alloca_val);
    }
  }
  else if (op == "jmp") {
    string target = args[0];
    llvm::BasicBlock* target_bb = (*bb_map)[target].first;
    builder->CreateBr(target_bb);
    (*bb_map)[target].second = true;
  }
  else if (op == "br") {
    llvm::Value* cond_ptr = (*val_map)[args[0]];
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
    llvm::Value* val_ptr = (*val_map)[args[0]];
    llvm::Value* val = builder->CreateLoad(val_ptr);
    if (val_map->count(dest)) {
      llvm::Value* dest_ptr = (*val_map)[dest];
      builder->CreateStore(val, dest_ptr);
    }
    else {
      llvm::Type* val_type = val->getType();
      llvm::Value* alloca_val = builder->CreateAlloca(val_type, llvm::ConstantInt::getSigned(t_int_, 1));
      (*val_map)[dest] = alloca_val;
      builder->CreateStore(val, alloca_val);
    }
  }
  else if (op == "print") {
    llvm::Value* val_ptr = (*val_map)[args[0]];
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
    cout << "here\n";
  }
  // do nothing for nop
  else if (op == "nop") ; 
  else 
    assert(false && "Operation not supported!\n");

}


// create a function from the json value v, place it in the module m, return its pointer
llvm::Function* createFunction(
  llvm::json::Value& v, 
  IRBuilder* builder, 
  llvm::LLVMContext* ctx,
  llvm::Module* m) {

  // some useful types
  llvm::Type* t_int_ = llvm::Type::getInt32Ty(*ctx);

  // maintain a map from basic blocks to pairs
  // each block has a flag indicating whether it is used or not
  BasicBlockMap_T bb_map;
  bb_map.clear();

  // maintain another map from variable names to LLVM values
  VarToVal_T val_map;
  val_map.clear();

  // convert the value to an object
  auto json_func = v.getAsObject();
  assert(json_func && "Failed to get object of function!\n");

  auto json_fname = json_func->getString("name");
  assert(json_fname && "The JSON object of the function is incomplete: missing name.\n");

  // get the name
  string fname = json_fname.getValue().str();
  cout << fname << endl;

  // create the function
  std::vector<llvm::Type*> arg_types;
  llvm::FunctionType* ftype = llvm::FunctionType::get(t_int_, arg_types, false);
  llvm::Function* f = llvm::Function::Create(ftype, llvm::Function::ExternalLinkage, fname, m);
  cout << "Function created!\n";

  // get all the instructions
  auto json_insts = json_func->getArray("instrs");
  assert(json_insts && "The JSON object of the function is incomplete: missing instrs.\n");

  // first iterate through all instructions, get all labels
  // notice that in Bril we can only jump to labels
  // so this is sufficient for tracking all branch and jump instructions
  for (auto inst = json_insts->begin(), inst_end = json_insts->end(); inst != inst_end; inst ++ ) {
    auto obj = inst->getAsObject();
    // special case for the first BB: if the function starts with a label, then name the 
    // BB with this label; otherwise name it "entry"
    if (auto json_bb_name = obj->getString("label")) {
      string bb_name = json_bb_name.getValue().str();
      llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, bb_name, f);
      // track the basic block, not used yet
      bb_map[bb_name] = BasicBlockFlag_T(bb, false);
    }
    else if (inst == json_insts->begin()){
      // right now we use empty string to indicate that this is the entry block
      // notice that in Bril, all useful basic blocks other than the entry block must have a name
      // here is an example:
      /*   ...
       *   br cond somewhere somewhere_else
       *   [ int: a = add b c ]
       * somewhere:
       *   int: a = add c d
       * somewhere_else:
       *   int: e = add c b
       *   ... 
      */
      // notice how the instruction embraced by square brackets never gets executed
      // the situation is similar for jmp and ret instructions
      llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, "", f);
      // track the basic block, not used yet
      bb_map[""] = BasicBlockFlag_T(bb, false);
    }
  }

  cout << "All basic blocks created!\n";

  // do it again, this time we try to insert instructions into the basic blocks
  for (auto inst = json_insts->begin(), inst_end = json_insts->end(); inst != inst_end; inst ++ ) {
    auto obj = inst->getAsObject();
    if (auto json_bb_name = obj->getString("label")) {
      // find the basic block
      string bb_name = json_bb_name.getValue().str();
      assert((bb_map.count(bb_name) > 0) && "Key is not found in basic block map!\n");
      llvm::BasicBlock* curr_bb = bb_map[bb_name].first;

      // check terminator of prev basic block
      if (inst != json_insts->begin()) {
        // get prev bb before insert to next one 
        llvm::BasicBlock* bb = builder->GetInsertBlock();
        // insert jmp to curr block if not well terminated 
        llvm::Instruction* term_inst = bb->getTerminator();
        if (term_inst == nullptr) {
            string target = bb_name;
            llvm::BasicBlock* target_bb = bb_map[target].first;
            builder->CreateBr(target_bb);
            bb_map[target].second = true;
        }
      }
      // set IRBuilder to insert stuff into this block
      builder->SetInsertPoint(curr_bb);
      // notice that this block may not be used yet!
    }
    else if (inst == json_insts->begin()) {
      // in this case the block is the entry block
      llvm::BasicBlock* curr_bb = bb_map[""].first;
      // it has to be used, since we will process the first instruction immediately
      bb_map[""].second = true;
      builder->SetInsertPoint(curr_bb);
      // process the first instruction
      createInst(builder, obj, &bb_map, &val_map);
    }
    else 
      createInst(builder, obj, &bb_map, &val_map);
  }
  // add a return at the end of the function, regardless of whether the function has a return or not
  builder->CreateRet(llvm::ConstantInt::get(t_int_, 0, true));

  return f;
}

int main(int argc, char** argv) {

  // sanity check
  if (argc < 3) {
    cout << "Usage: ./bril_llvm <input_file> <output_file>" << endl;
    return 1;
  }

  // read in the file
  ifstream json_file;
  string json_str;
  stringstream json_stream;
  json_file.open(string(argv[1]), ios::in);
  if (json_file.is_open()) {
    json_stream << json_file.rdbuf();
    json_str = json_stream.str();
  }
  else {
    cout << "Input JSON file " << string(argv[1]) << " does not exist!" << endl;
    return 2;
  }

  // parse the jason file
  auto json_val = llvm::json::parse(json_str);
  if (auto err = json_val.takeError()) {
    cout << "JSON parsing error!" << endl;
    return 3;
  }

  // create an LLVM context
  std::shared_ptr<llvm::LLVMContext> ctx_ = std::make_shared<llvm::LLVMContext>();
  // create an LLVM IRBuilder
  std::unique_ptr<IRBuilder> builder_;
  builder_.reset(new IRBuilder(*ctx_));
  // create an LLVM module
  std::unique_ptr<llvm::Module> module_;
  module_.reset(new llvm::Module("bril_llvm", *ctx_));
  llvm::Module* module = module_.get();

  // try to parse the llvm::json::Value
  // a simple example to retrieve the "name" field
  if (llvm::json::Object* O = json_val->getAsObject()) {
    // the whole program should be a json object (in { } )
    if (llvm::json::Array* fa = O->getArray("functions")) {
      // the object should have one key called functions
      // this key should map to a json array containing all functions (in [ ] )
      // currently we only have one function
      for (int i = 0; i < fa->size(); i ++ ) {
        cout << "Create function ... " << endl;
        llvm::Function* f = createFunction((*fa)[i], builder_.get(), ctx_.get(), module);
        if (!f) {
          cout << "Create function failed. " << endl;
          return 5;
        }
      }
    }
  }

  std::error_code ecode;
  llvm::raw_fd_ostream dest(string(argv[2]), ecode, llvm::sys::fs::F_None);
  module->print(dest, nullptr);
  cout << "dump code\n";

  // execute the function
  // first, initialize LLVM environment
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs(); 
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  // create EngineBuilder && ExecutionEngine
  llvm::EngineBuilder builder(std::move(module_));
  builder.setEngineKind(llvm::EngineKind::JIT);
  llvm::ExecutionEngine* ee = builder.create();
  // run the main function
  func_t func = (func_t)(ee->getFunctionAddress("main"));
  (*func)();
  delete ee;
  return 0;
}
