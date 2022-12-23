#ifndef SYSY_COMPILER_LOOP_DELETION_H
#define SYSY_COMPILER_LOOP_DELETION_H

#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"

namespace llvm {

    class LoopDeletionPass : public PassInfoMixin<LoopDeletionPass> {
    public:
        LoopDeletionPass() = default;

        PreservedAnalyses run(Loop &L, LoopAnalysisManager &AM,
                              LoopStandardAnalysisResults &AR, LPMUpdater &U);
    };

}
#endif
