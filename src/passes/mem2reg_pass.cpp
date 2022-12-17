#include <mem2reg_pass.h>
#include <llvm/ADT/Statistic.h>
#include <llvm/Analysis/AssumptionCache.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/InitializePasses.h>
#include <llvm/Pass.h>
#include <llvm/Support/Casting.h>
#include <llvm/Transforms/Utils.h>
//#include <llvm/Transforms/Utils/PromoteMemToReg.h>
#include <mem2reg_pass_helper.h>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "mem2reg"

// 传递在此DEBUG_TYPE下提升了的数量信息
STATISTIC(numPro, "Number of alloca's promoted");

// 入口函数
static bool promoteMemoryToRegister(Function &F, DominatorTree &DT, AssumptionCache &AC) {
    std::vector<AllocaInst *> allocas;
    BasicBlock &entryBB = F.getEntryBlock();
    bool isChange = false;

    while (true) {
        allocas.clear();

        // 遍历所有alloc指令，其中promotable的放到vector中
        for (BasicBlock::iterator st = entryBB.begin(), ed = --entryBB.end(); st != ed; ++st)
            if (AllocaInst *inst = dyn_cast<AllocaInst>(st)) // Is it an alloca?
                if (isAllocaPromotable(inst))
                    allocas.push_back(inst);

        if (!allocas.size())
            break;

        // 将promotable的局部变量由内存放入寄存器中，即实施SSA构造算法
        PromoteMemToReg(allocas, DT, &AC);
        isChange = true;
        numPro += allocas.size();
    }
    return isChange;
}

PreservedAnalyses PromotePass::run(Function &F, FunctionAnalysisManager &AM) {
    auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
    auto &AC = AM.getResult<AssumptionAnalysis>(F);
    if (!promoteMemoryToRegister(F, DT, AC))
        return PreservedAnalyses::all();

    PreservedAnalyses PA;
    PA.preserveSet<CFGAnalyses>();
    return PA;
}

namespace {
    // 函数级的pass
    struct PromoteLegacyPass : public FunctionPass {
        static char ID;

        PromoteLegacyPass() : FunctionPass(ID) {
            initializePromoteLegacyPassPass(*PassRegistry::getPassRegistry());
        }

        bool runOnFunction(Function &F) override {
            // 判断是否开启了pass优化
            if (skipFunction(F))
                return false;

            // 建立支配者树和假设缓存后，走入口函数
            DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
            AssumptionCache &AC =
                    getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
            return promoteMemoryToRegister(F, DT, AC);
        }

        void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.addRequired<AssumptionCacheTracker>();
            AU.addRequired<DominatorTreeWrapperPass>();
            AU.setPreservesCFG();
        }
    };

}

char PromoteLegacyPass::ID = 0;

INITIALIZE_PASS_BEGIN(PromoteLegacyPass, "mem2reg", "Promote Memory to "
                                                    "Register",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(PromoteLegacyPass, "mem2reg", "Promote Memory to Register",
                    false, false)

FunctionPass *llvm::createPromoteMemoryToRegisterPass() {
    return new PromoteLegacyPass();
}
