#include <iostream>
#include "common.h"

using namespace std;

int main(void) {

  // parse the jason file

  // create an LLVM context
  std::shared_ptr<llvm::LLVMContext> ctx_ = std::make_shared<llvm::LLVMContext>();
  // create an LLVM IRBuilder
  using IRBuilder = llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>;
  std::unique_ptr<IRBuilder> builder_;
  builder_.reset(new IRBuilder(*ctx_));
  // create an LLVM module
  std::unique_ptr<llvm::Module> module_;
  module_.reset(new llvm::Module("bril_llvm", *ctx_));
  llvm::Module* module = module_.get();

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
  llvm::raw_fd_ostream dest("test.ll", ecode, llvm::sys::fs::F_None);
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
