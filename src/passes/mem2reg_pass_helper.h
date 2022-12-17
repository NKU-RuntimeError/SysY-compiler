#ifndef SYSY_COMPILER_MEM2REG_PASS_HELPER_H
#define SYSY_COMPILER_MEM2REG_PASS_HELPER_H

namespace llvm {

    template <typename T> class ArrayRef;
    class AllocaInst;
    class DominatorTree;
    class AssumptionCache;
    bool isAllocaPromotable(const AllocaInst *AI);
    void PromoteMemToReg(ArrayRef<AllocaInst *> Allocas, DominatorTree &DT,
                         AssumptionCache *AC = nullptr);

}

#endif
