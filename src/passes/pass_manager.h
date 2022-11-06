#ifndef SYSY_COMPILER_PASSES_PASS_MANAGER_H
#define SYSY_COMPILER_PASSES_PASS_MANAGER_H

#include <llvm/Passes/PassBuilder.h>

namespace PassManager {
    void run(llvm::OptimizationLevel level);
}

#endif //SYSY_COMPILER_PASSES_PASS_MANAGER_H
