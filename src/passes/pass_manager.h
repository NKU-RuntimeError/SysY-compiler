#ifndef SYSY_COMPILER_PASSES_PASS_MANAGER_H
#define SYSY_COMPILER_PASSES_PASS_MANAGER_H

#include <llvm/Passes/PassBuilder.h>

namespace PassManager {
    void run(int optLevel, const std::string &filename);
}

#endif //SYSY_COMPILER_PASSES_PASS_MANAGER_H
