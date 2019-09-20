#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <utility>
#include <assert>
#include "common.h"
#include "llvm/Support/JSON.h"

using namespace std;
using IRBuilder = llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>;

using BasicBlockFlag_T = pair<llvm::Basicblock*, bool> ;
using BasicBlockMap_T = map<string, BasicBlockFlag_T>;

// create an instruction from the json object, place it into a basic block using IRBuilder
void createInst(
  IRBuilder* builder,
  llvm::json::Object* obj,
  BasicBlockMap_T* bbMap) {

  auto json_op = obj->getString("op");
  assert(json_op && "Instruction missing field op!\n");

  string op = json_op.getValue().str();

  if (op == "const") {
  }
  else if (op == "add") {
  }
  else if (op == "mul") {
  }
  else if (op == "sub") {
  }
  else if (op == "div") {
  }
  else if (op == "eq") {
  }
  else if (op == "lt") {
  }
  else if (op == "gt") {
  }
  else if (op == "not") {
  }
  else if (op == "and") {
  }
  else if (op == "or") {
  }
  else if (op == "jmp") {
  }
  else if (op == "br") {
  }
  else if (op == "ret") {
  }
  else if (op == "id") {
  }
  else if (op == "print") {
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
  llvm::Type* t_void_ = llvm::Type::getVoidTy(*ctx);
  llvm::Type* t_int_ = llvm::Type::getInt32Ty(*ctx);
  llvm::Type* t_char_ = llvm::Type::getInt8Ty(*ctx);
  llvm::Type* t_char_p_ = llvm::Type::getInt8Ty(*ctx)->getPointerTo();

  // maintain a map of basic blocks, each block has a flag indicating whether it is used or not
  BasicBlockMap_T bbMap;
  bbMap.clear();

  // convert the value to an object
  auto json_func = v.getAsObject();
  assert(json_func && "Failed to get object of function!\n");

  auto json_fname = json_func->getString("name");
  assert(json_name && "The JSON object of the function is incomplete: missing name.\n");

  // get the name
  string fname = json_fname.getValue().str();

  // create the function
  std::vector<llvm::Type*> arg_types;
  llvm::FunctionType* ftype = llvm::FunctionType::get(t_int_, arg_types, false);
  llvm::Function* f = llvm::Function::Create(ftype, llvm::Function::ExternalLinkage, fname, m);

  // get all the instructions
  auto json_insts = json_func->getArray("instrs");
  assert(json_insts && "The JSON object of the function is incomplete: missing instrs.\n");

  // first iterate through all instructions, get all labels
  // notice that in Bril we can only jump to labels
  // so this is sufficient for tracking all branch and jump instructions
  for (auto inst = json_insts.begin(), inst_end = json_insts.end(); inst != inst_end; inst ++ ) {
    auto obj = inst->getAsObject();
    // special case for the first BB: if the function starts with a label, then name the 
    // BB with this label; otherwise name it "entry"
    if (auto json_bb_name = obj->getString("label")) {
      string bb_name = json_bb_name.getValue().str();
      llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, bb_name, f);
      // track the basic block, not used yet
      bbMap[bb_name] = BasicBlockFlag_T(bb, false);
    }
    else if (inst == json_insts.begin()){
      // right now we use this special string to indicate that this is the entry block
      llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, "", f);
      // track the basic block, not used yet
      bbMap[bb_name] = BasicBlockFlag_T(bb, false);
    }
  }

  // do it again, this time we try to insert instructions into the basic blocks
  for (auto inst = json_insts.begin(), inst_end = json_insts.end(); inst != inst_end; inst ++ ) {
    auto obj = inst->getAsObject();
    if (auto json_bb_name = obj->getString("label")) {
      // find the basic block
      string bb_name = json_bb_name.getValue().str();
      assert((bbMap.count(bb_name) > 0) && "Key is not found in basic block map!\n";
      llvm::BasicBlock* curr_bb = bbMap[bb_name].first;
      // set IRBuilder to insert stuff into this block
      builder->SetInsertPoint(curr_bb);
      // notice that this block may not be used yet!
    }
    else if (inst == json_insts.begin()){
      // in this case the block is the entry block
      llvm::BasicBlock* curr_bb = bbMap[""].first;
      // it has to be used, since we will process the first instruction immediately
      bbMap[""].second = true;
      builder->SetInsertPoint(curr_bb);
      // process the first instruction
      createInst(builder, inst, &bbMap);
    }
    else 
      createInst(builder, inst, &bbMap);
  }

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
        llvm::Function* f = createFunction((*fa)[i], builder_, ctx, module);
        if (!f) {
          cout << "Create function failed. " << endl;
          return 5;
        }
      }
    }
  }

  // use IRBuilder to build LLVM runtime
  // usefule types
  llvm::Type* t_void_ = llvm::Type::getVoidTy(*ctx_);
  llvm::Type* t_int_ = llvm::Type::getInt32Ty(*ctx_);
  llvm::Type* t_char_ = llvm::Type::getInt8Ty(*ctx_);
  llvm::Type* t_char_p_ = llvm::Type::getInt8Ty(*ctx_)->getPointerTo();
  // first, create a main function
  std::vector<llvm::Type*> arg_types;
  llvm::FunctionType* ftype = llvm::FunctionType::get(t_int_, arg_types, false);
  llvm::Function* function_ = llvm::Function::Create(ftype, llvm::Function::ExternalLinkage, "my_main", module_.get());
  // start the fist BB
  llvm::BasicBlock* entry = llvm::BasicBlock::Create(*ctx_, "entry", function_);
  builder_->SetInsertPoint(entry);
  // print a number
  std::vector<llvm::Type*> call_types;
  call_types.push_back(t_char_p_);
  call_types.push_back(t_int_);
  llvm::FunctionType* call_ftype = llvm::FunctionType::get(t_int_, call_types, false);
  llvm::Function* printf_call = llvm::cast<llvm::Function>(module->getOrInsertFunction("printf", call_ftype));
  std::vector<llvm::Value*> args;
  args.push_back(builder_->CreateGlobalStringPtr("Hi, I am %d.\n"));
  args.push_back(llvm::ConstantInt::getSigned(t_int_, 10));
  builder_->CreateCall(printf_call, args);
  // return 0 for the main function
  builder_->CreateRet(llvm::ConstantInt::getSigned(t_int_, 0));

  std::error_code ecode;
  llvm::raw_fd_ostream dest(string(argv[2]), ecode, llvm::sys::fs::F_None);
  module->print(dest, nullptr);

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
  func_t func = (func_t)(ee->getFunctionAddress("my_main"));
  (*func)();
  delete ee;
  return 0;
}
