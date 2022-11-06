#ifndef SYSY_COMPILER_PASSES_HELLO_WORLD_PASS_H
#define SYSY_COMPILER_PASSES_HELLO_WORLD_PASS_H

#include <llvm/IR/PassManager.h>

class HelloWorldPass : public llvm::PassInfoMixin<HelloWorldPass> {
public:
    llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &AM);
};

#endif //SYSY_COMPILER_PASSES_HELLO_WORLD_PASS_H
