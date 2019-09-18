#include "common.h"

int main(void) {

  // parse the jason file

  // create an LLVM context
  std::shared_ptr<llvm::LLVMContext> ctx_ = std::make_shared<llvm::LLVMContext>();
  llvm::LLVMContext* ctx = ctx_.get();
  // create an LLVM IRBuilder
  using IRBuilder = llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>;
  std::unique_ptr<IRBuilder> builder_;
  builder_.reset(new IRBuilder(*ctx_));
  // create an LLVM module
  std::unique_ptr<llvm::Module> module_;
  module_.reset(new llvm::Module("bril_llvm", *ctx_));

  // use IRBuilder to build LLVM runtime


}
