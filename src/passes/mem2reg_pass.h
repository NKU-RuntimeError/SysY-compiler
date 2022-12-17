#ifndef SYSY_COMPILER_MEM2REG_PASS_H
#define SYSY_COMPILER_MEM2REG_PASS_H

#include <llvm/IR/PassManager.h>

namespace llvm {

    class Function;

    class PromotePass : public PassInfoMixin<PromotePass> {
    public:
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    };
}

#endif
