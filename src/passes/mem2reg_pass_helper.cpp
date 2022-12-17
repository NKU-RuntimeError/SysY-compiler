#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/Statistic.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Analysis/AssumptionCache.h>
#include <llvm/Analysis/InstructionSimplify.h>
#include <llvm/Analysis/IteratedDominanceFrontier.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/User.h>
#include <llvm/Support/Casting.h>
#include <mem2reg_pass_helper.h>
#include <cassert>
#include <utility>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "mem2reg"

STATISTIC(NumLocalPromoted, "Number of alloca's promoted within one block");
STATISTIC(NumSingleStore,   "Number of alloca's promoted with a single store");
STATISTIC(NumDeadAlloca,    "Number of dead alloca's removed");
STATISTIC(NumPHIInsert,     "Number of PHI nodes inserted");

// 判断是否能从内存提升到寄存器
// 不可提升的情况：volatile属性的变量、有被取址操作的局部变量
// 实验里面不涉及volatile的情况，所以不做处理
bool llvm::isAllocaPromotable(const AllocaInst *AI) {
    for (const User *U : AI->users()) {
        // load: load指令的类型和alloc指令的类型不符，无法提升
        if (const LoadInst *LI = dyn_cast<LoadInst>(U)) {
            if (LI->getType() != AI->getAllocatedType())
                return false;
            // store:不允许alloc的结果作为store的左操作数，仅能做右操作数，即被存储的对象；load指令的类型和alloc指令的类型不符，无法提升
        } else if (const StoreInst *SI = dyn_cast<StoreInst>(U)) {
            if (SI->getValueOperand() == AI || SI->getValueOperand()->getType() != AI->getAllocatedType())
                return false;
            // GEP：有一个索引非0（结果指针不等同于第一个操作数），或有一个使用到该指令的user不是生存期标志/可遗弃的，无法提升
        } else if (const GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(U)) {
            if (!GEPI->hasAllZeroIndices() || !onlyUsedByLifetimeMarkersOrDroppableInsts(GEPI))
                return false;
        } else {
            return false;
        }
    }
    return true;
}

namespace {

    struct AllocaInfo {
        using DbgUserVec = SmallVector<DbgVariableIntrinsic *, 1>;

        SmallVector<BasicBlock *, 32> DefiningBlocks;
        SmallVector<BasicBlock *, 32> UsingBlocks;

        StoreInst *OnlyStore;
        BasicBlock *OnlyBlock;
        bool OnlyUsedInOneBlock;

        void clear() {
            DefiningBlocks.clear();
            UsingBlocks.clear();
            OnlyStore = nullptr;
            OnlyBlock = nullptr;
            OnlyUsedInOneBlock = true;
        }

        void AnalyzeAlloca(AllocaInst *AI) {
            clear();

            // 获得store指令和load指令所在的基本块，并判断它们是否在同一块中
            for (User *U : AI->users()) {
                Instruction *User = cast<Instruction>(U);

                if (StoreInst *SI = dyn_cast<StoreInst>(User)) {
                    DefiningBlocks.push_back(SI->getParent());
                    OnlyStore = SI;
                } else {
                    LoadInst *LI = cast<LoadInst>(User);
                    UsingBlocks.push_back(LI->getParent());
                }

                if (OnlyUsedInOneBlock) {
                    if (!OnlyBlock)
                        OnlyBlock = User->getParent();
                    else if (OnlyBlock != User->getParent())
                        OnlyUsedInOneBlock = false;
                }
            }
        }
    };

    // 定义Renamepass的数据包结构，包括PHI节点所在的块、流入块、流入值等
    struct RenamePassData {
        using ValVector = std::vector<Value *>;
        using LocationVector = std::vector<DebugLoc>;

        RenamePassData(BasicBlock *B, BasicBlock *P, ValVector V, LocationVector L)
                : BB(B), Pred(P), Values(std::move(V)), Locations(std::move(L)) {}

        BasicBlock *BB;
        BasicBlock *Pred;
        ValVector Values;
        LocationVector Locations;
    };

    // 获取每个块中load/store指令的位置顺序关系，可以避免扫描很多大的基本块
    class LargeBlockInfo {
        // 记录序号，表示位置顺序
        DenseMap<const Instruction *, unsigned> InstNumbers;

    public:

        static bool isInterestingInstruction(const Instruction *I) {
            return (isa<LoadInst>(I) && isa<AllocaInst>(I->getOperand(0))) ||
                   (isa<StoreInst>(I) && isa<AllocaInst>(I->getOperand(1)));
        }

        // 计算每个指令的顺序序号
        unsigned getInstructionIndex(const Instruction *I) {
            assert(isInterestingInstruction(I) &&
                   "Not a load/store to/from an alloca?");

            DenseMap<const Instruction *, unsigned>::iterator It = InstNumbers.find(I);
            if (It != InstNumbers.end())
                return It->second;

            const BasicBlock *BB = I->getParent();
            unsigned InstNo = 0;
            for (const Instruction &BBI : *BB)
                if (isInterestingInstruction(&BBI))
                    InstNumbers[&BBI] = InstNo++;
            It = InstNumbers.find(I);

            assert(It != InstNumbers.end() && "Didn't insert instruction?");
            return It->second;
        }

        void deleteValue(const Instruction *I) { InstNumbers.erase(I); }

        void clear() { InstNumbers.clear(); }
    };

    struct PromoteMem2Reg {
        // 可提升的alloca指令
        std::vector<AllocaInst *> Allocas;

        DominatorTree &DT;
        DIBuilder DIB;
        AssumptionCache *AC;

        const SimplifyQuery SQ;

        DenseMap<AllocaInst *, unsigned> AllocaLookup;

        // 本pass所新插入的PHI节点，一个PHI节点中有两个操作数
        DenseMap<std::pair<unsigned, unsigned>, PHINode *> NewPhiNodes;

        DenseMap<PHINode *, unsigned> PhiToAllocaMap;

        // 记录已经访问过的基本块，避免重复访问
        SmallPtrSet<BasicBlock *, 16> Visited;

        // 记录基本块的序号
        DenseMap<BasicBlock *, unsigned> BBNumbers;

        // 记录基本块有几个前继块
        DenseMap<const BasicBlock *, unsigned> BBNumPreds;

    public:
        PromoteMem2Reg(ArrayRef<AllocaInst *> Allocas, DominatorTree &DT,
                       AssumptionCache *AC)
                : Allocas(Allocas.begin(), Allocas.end()), DT(DT),
                  DIB(*DT.getRoot()->getParent()->getParent(), /*AllowUnresolved*/ false),
                  AC(AC), SQ(DT.getRoot()->getParent()->getParent()->getDataLayout(),
                             nullptr, &DT, AC) {}

        void run();

    private:
        void RemoveFromAllocasList(unsigned &AllocaIdx) {
            Allocas[AllocaIdx] = Allocas.back();
            Allocas.pop_back();
            --AllocaIdx;
        }

        unsigned getNumPreds(const BasicBlock *BB) {
            unsigned &NP = BBNumPreds[BB];
            if (NP == 0)
                NP = pred_size(BB) + 1;
            return NP - 1;
        }

        void ComputeLiveInBlocks(AllocaInst *AI, AllocaInfo &Info,
                                 const SmallPtrSetImpl<BasicBlock *> &DefBlocks,
                                 SmallPtrSetImpl<BasicBlock *> &LiveInBlocks);
        void RenamePass(BasicBlock *BB, BasicBlock *Pred,
                        RenamePassData::ValVector &IncVals,
                        RenamePassData::LocationVector &IncLocs,
                        std::vector<RenamePassData> &Worklist);
        bool QueuePhiNode(BasicBlock *BB, unsigned AllocaIdx, unsigned &Version);
    };

}

// 初始化时假设load指令都非空
static void addAssumeNonNull(AssumptionCache *AC, LoadInst *LI) {
    Function *AssumeIntrinsic =
            Intrinsic::getDeclaration(LI->getModule(), Intrinsic::assume);
    ICmpInst *LoadNotNull = new ICmpInst(ICmpInst::ICMP_NE, LI,
                                         Constant::getNullValue(LI->getType()));
    LoadNotNull->insertAfter(LI);
    CallInst *CI = CallInst::Create(AssumeIntrinsic, {LoadNotNull});
    CI->insertAfter(LoadNotNull);
    AC->registerAssumption(cast<AssumeInst>(CI));
}

// alloca指令可提升的情况下可删除除了load和store以外使用它的指令
static void removeIntrinsicUsers(AllocaInst *AI) {

    for (Use &U : llvm::make_early_inc_range(AI->uses())) {
        Instruction *I = cast<Instruction>(U.getUser());
        if (isa<LoadInst>(I) || isa<StoreInst>(I))
            continue;

        if (I->isDroppable()) {
            I->dropDroppableUse(U);
            continue;
        }

        if (!I->getType()->isVoidTy()) {
            for (Use &UU : llvm::make_early_inc_range(I->uses())) {
                Instruction *Inst = cast<Instruction>(UU.getUser());

                if (Inst->isDroppable()) {
                    Inst->dropDroppableUse(UU);
                    continue;
                }
                Inst->eraseFromParent();
            }
        }
        I->eraseFromParent();
    }
}

// 只有一个store语句，那么被这个store指令所支配的所有load都要被替换为store语句中的右值。
static bool rewriteSingleStoreAlloca(AllocaInst *AI, AllocaInfo &Info,
                                     LargeBlockInfo &LBI, const DataLayout &DL,
                                     DominatorTree &DT, AssumptionCache *AC) {
    StoreInst *OnlyStore = Info.OnlyStore;
    bool StoringGlobalVal = !isa<Instruction>(OnlyStore->getOperand(0));
    BasicBlock *StoreBB = OnlyStore->getParent();
    int StoreIndex = -1;

    // 清空原本的使用(load)块
    Info.UsingBlocks.clear();

    for (User *U : make_early_inc_range(AI->users())) {

        Instruction *UserInst = cast<Instruction>(U);
        if (UserInst == OnlyStore)
            continue;
        LoadInst *LI = cast<LoadInst>(UserInst);

        // 首先确保是alloca出的局部变量
        if (!StoringGlobalVal) {
            if (LI->getParent() == StoreBB) {
                // 然后根据索引判断load是否被store所支配
                if (StoreIndex == -1)
                    StoreIndex = LBI.getInstructionIndex(OnlyStore);

                // load在store之前，load不做处理，重新放回UsingBlocks中
                if (unsigned(StoreIndex) > LBI.getInstructionIndex(LI)) {
                    Info.UsingBlocks.push_back(StoreBB);
                    continue;
                }
            } else if (!DT.dominates(StoreBB, LI->getParent())) {
                // 不在同一个块中，如果store不支配这个load，则也放弃处理
                Info.UsingBlocks.push_back(LI->getParent());
                continue;
            }
        }

        // 否则就替换掉load
        Value *ReplVal = OnlyStore->getOperand(0);
        // store支配load但流程上load出现在store之前，说明存在一个不经过函数入口的路径先到达了load再到store，这时假设是undef，因为函数不可能不从入口进入
        if (ReplVal == LI)
            ReplVal = PoisonValue::get(LI->getType());

        if (AC && LI->getMetadata(LLVMContext::MD_nonnull) &&
            !isKnownNonZero(ReplVal, DL, 0, AC, LI, &DT))
            addAssumeNonNull(AC, LI);

        // 替换掉load的所有user
        LI->replaceAllUsesWith(ReplVal);
        LI->eraseFromParent();
        LBI.deleteValue(LI);
    }

    // 没有load说明完成了
    if (Info.UsingBlocks.size())
        return false;

    // 移除本此处理好的store和alloca
    Info.OnlyStore->eraseFromParent();
    LBI.deleteValue(Info.OnlyStore);

    AI->eraseFromParent();
    return true;
}

// 如果某局部变量的读/写(load/store)都只存在一个基本块中，load要被之前离他最近的store的右值替换
static bool promoteSingleBlockAlloca(AllocaInst *AI, const AllocaInfo &Info,
                                     LargeBlockInfo &LBI,
                                     const DataLayout &DL,
                                     DominatorTree &DT,
                                     AssumptionCache *AC) {
    // 找到所有store指令的顺序
    using StoresByIndexTy = SmallVector<std::pair<unsigned, StoreInst *>, 64>;
    StoresByIndexTy StoresByIndex;

    for (User *U : AI->users())
        if (StoreInst *SI = dyn_cast<StoreInst>(U))
            StoresByIndex.push_back(std::make_pair(LBI.getInstructionIndex(SI), SI));

    // 对store的index进行排序，用二分法找最近的store
    llvm::sort(StoresByIndex, less_first());

    // 遍历所有load指令，用前面最近的store指令替换掉
    for (User *U : make_early_inc_range(AI->users())) {
        LoadInst *LI = dyn_cast<LoadInst>(U);
        if (!LI)
            continue;

        unsigned LoadIdx = LBI.getInstructionIndex(LI);

        // 找到离load最近的store
        StoresByIndexTy::iterator I = llvm::lower_bound(
                StoresByIndex,
                std::make_pair(LoadIdx, static_cast<StoreInst *>(nullptr)),
                less_first());
        Value *ReplVal;
        if (I == StoresByIndex.begin()) {
            if (!StoresByIndex.size())
                // 如果有store，用store的操作数替换load的user
                ReplVal = UndefValue::get(LI->getType());
            else
                return false;
        } else {
            ReplVal = std::prev(I)->second->getOperand(0);
        }

        if (AC && LI->getMetadata(LLVMContext::MD_nonnull) &&
            !isKnownNonZero(ReplVal, DL, 0, AC, LI, &DT))
            addAssumeNonNull(AC, LI);
        if (ReplVal == LI)
            ReplVal = PoisonValue::get(LI->getType());

        // 替换load指令
        LI->replaceAllUsesWith(ReplVal);
        LI->eraseFromParent();
        LBI.deleteValue(LI);
    }

    ++NumLocalPromoted;
    return true;
}

// 将局部变量由内存提升到寄存器的主体函数
void PromoteMem2Reg::run() {
    Function &F = *DT.getRoot()->getParent();

    AllocaInfo Info;
    LargeBlockInfo LBI;
    ForwardIDFCalculator IDF(DT);

    for (unsigned allocaNum = 0; allocaNum != Allocas.size(); ++allocaNum) {
        AllocaInst *AI = Allocas[allocaNum];

        // 前置条件检验
        assert(isAllocaPromotable(AI) && "Cannot promote non-promotable alloca!");
        assert(AI->getParent()->getParent() == &F && "All allocas should be in the same function, which is same as DF!");

        removeIntrinsicUsers(AI);

        // 三种优化，都很好理解
        // 优化1：如果alloca出的空间从未被使用，直接删除
        if (AI->use_empty()) {
            AI->eraseFromParent();
            RemoveFromAllocasList(allocaNum);
            ++NumDeadAlloca;
            continue;
        }

        // 计算哪里的基本块定义(store)和使用(load)了alloc变量
        Info.AnalyzeAlloca(AI);

        // 优化2：只有一个store语句，且只存在一个基本块中，那么被这个store基本块所支配的所有load都要被替换为store语句中的右值。
        if (Info.DefiningBlocks.size() == 1) {
            if (rewriteSingleStoreAlloca(AI, Info, LBI, SQ.DL, DT, AC)) {
                RemoveFromAllocasList(allocaNum);
                ++NumSingleStore;
                continue;
            }
        }

        // 优化3：如果某局部变量的读/写(load/store)都只存在一个基本块中，load要被之前离他最近的store的右值替换
        if (Info.OnlyUsedInOneBlock && promoteSingleBlockAlloca(AI, Info, LBI, SQ.DL, DT, AC)) {
            RemoveFromAllocasList(allocaNum);
            continue;
        }

        // 没给基本块编号的话就编一下
        if (BBNumbers.empty()) {
            int id = 0;
            for (BasicBlock &BB : F)
                BBNumbers[&BB] = id++;
        }

        // 在重命名pass优化时要保持allocas中的alloc到其下标的映射
        AllocaLookup[Allocas[allocaNum]] = allocaNum;

        // 设置唯一的一组定义块，用于高效查找
        SmallPtrSet<BasicBlock *, 32> defBlocks(Info.DefiningBlocks.begin(), Info.DefiningBlocks.end());

        // 计算alloca存活的基本块，不存活的后续不必插入PHI节点，起简化作用
        // 判断一个基本块内是否在load之前store了，如果有，说明不是live in的，因为原来的alloca变量被覆盖了
        SmallPtrSet<BasicBlock *, 32> liveInBlocks;
        ComputeLiveInBlocks(AI, Info, defBlocks, liveInBlocks);

        // 使用IDF算法计算支配边界，确认哪些块确实需要插入PHI节点，即为PHIBlocks
        // IDF算法主要优化了支配者树的计算，其实就是在传统的严格支配的边D-Edge之外添加非严格支配的变J-Edge，简化寻找支配边界的过程
        IDF.setLiveInBlocks(liveInBlocks);
        IDF.setDefiningBlocks(defBlocks);
        SmallVector<BasicBlock *, 32> PHIBlocks;
        IDF.calculate(PHIBlocks);
        llvm::sort(PHIBlocks, [this](BasicBlock *A, BasicBlock *B) {
            return BBNumbers.find(A)->second < BBNumbers.find(B)->second;
        });

        // 在PHIBlocks的开头插入PHI节点，建立PHI-alloca的映射关系
        unsigned ver = 0;
        for (BasicBlock *BB : PHIBlocks)
            QueuePhiNode(BB, allocaNum, ver);

        //至此，已构造好了空的PHI节点，并知道要在哪里插入这些PHI节点
    }

    if (!Allocas.size())
        return;
    LBI.clear();

    // 为所有value设置一个隐式的初始定义，避免出现使用未定义value的情况。所谓name就是def定义的意思
    RenamePassData::ValVector values(Allocas.size());
    for (unsigned i = 0, e = Allocas.size(); i != e; ++i)
        values[i] = UndefValue::get(Allocas[i]->getAllocatedType());

    // 初始化传递位置，假设所有传入值的来源位置都未知
    RenamePassData::LocationVector Locations(Allocas.size());

    // 众所周知PHI节点中要有流入数据的信息，此处用rename进行添加
    // RenamePass做两件事：1.在空PHI节点中加入流入数据 2.处理替换掉load和store指令
    std::vector<RenamePassData> RenamePassWorkList;
    RenamePassWorkList.emplace_back(&F.front(), nullptr, std::move(values),
                                    std::move(Locations));
    do {
        RenamePassData RPD = std::move(RenamePassWorkList.back());
        RenamePassWorkList.pop_back();
        RenamePass(RPD.BB, RPD.Pred, RPD.Values, RPD.Locations, RenamePassWorkList);
    } while (RenamePassWorkList.size());

    Visited.clear();

    // rename处理掉了load和store，现在处理掉alloca
    for (Instruction *A : Allocas) {
        // 删除要求：在所有可访问的块中，都不再有对alloca的使用
        if (!A->use_empty())
            A->replaceAllUsesWith(PoisonValue::get(A->getType()));
        A->eraseFromParent();
    }

    // PHI优化，处理不同来源的流入值都相同的情况，此时没必要用PHI了
    // 需要循环迭代进行，因为一个PHI节点删了会影响其他
    bool EliminatedAPHI = true;
    while (EliminatedAPHI) {
        // 如果没有能删了的就退出
        EliminatedAPHI = false;

        // 迭代通过mem2reg pass新添加的所有PHI节点
        for (DenseMap<std::pair<unsigned, unsigned>, PHINode *>::iterator
                     I = NewPhiNodes.begin(),
                     E = NewPhiNodes.end();
             I != E;) {
            PHINode *PN = I->second;

            // 如果有流入值都相同的情况，用此流入值替换PHI节点
            if (Value *V = SimplifyInstruction(PN, SQ)) {
                PN->replaceAllUsesWith(V);
                PN->eraseFromParent();
                NewPhiNodes.erase(I++);
                EliminatedAPHI = true;
                continue;
            }
            ++I;
        }
    }

    NewPhiNodes.clear();
}

// 计算alloca存活的基本块
void PromoteMem2Reg::ComputeLiveInBlocks(
        AllocaInst *AI, AllocaInfo &Info,
        const SmallPtrSetImpl<BasicBlock *> &DefBlocks,
        SmallPtrSetImpl<BasicBlock *> &LiveInBlocks) {
    SmallVector<BasicBlock *, 64> LiveInBlockWorklist(Info.UsingBlocks.begin(),
                                                      Info.UsingBlocks.end());

    // 检测出现store的block中，store是否在load前，如果是，则说明不是live in的
    for (unsigned i = 0, e = LiveInBlockWorklist.size(); i != e; ++i) {
        BasicBlock *BB = LiveInBlockWorklist[i];
        if (!DefBlocks.count(BB))
            continue;
        // BB中既有load又有store，迭代指令确定第一个load和第一个store的相对位置
        for (BasicBlock::iterator I = BB->begin();; ++I) {
            if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
                if (SI->getOperand(1) != AI)
                    continue;
                LiveInBlockWorklist[i] = LiveInBlockWorklist.back();
                LiveInBlockWorklist.pop_back();
                --i;
                --e;
                break;
            }

            if (LoadInst *LI = dyn_cast<LoadInst>(I))
                // 在store前遇到了load，说明是live in
                if (LI->getOperand(0) == AI)
                    break;
        }
    }

   // // 迭代的添加live in的前继结点到live in集合中
    while (!LiveInBlockWorklist.empty()) {
        BasicBlock *BB = LiveInBlockWorklist.pop_back_val();

        // 第二次遇到不再重复处理
        if (!LiveInBlocks.insert(BB).second)
            continue;

        // 如果前继有def，那就不需要继续添加前继的前继
        for (BasicBlock *P : predecessors(BB)) {
            if (DefBlocks.count(P))
                continue;

            LiveInBlockWorklist.push_back(P);
        }
    }
}

// 在block的开头插入PHI节点，先处理PHI节点再处理其他指令
bool PromoteMem2Reg::QueuePhiNode(BasicBlock *BB, unsigned AllocaNo,
                                  unsigned &Version) {
    PHINode *&PN = NewPhiNodes[std::make_pair(BBNumbers[BB], AllocaNo)];

    if (PN)
        return false;

    PN = PHINode::Create(Allocas[AllocaNo]->getAllocatedType(), getNumPreds(BB),
                         Allocas[AllocaNo]->getName() + "." + Twine(Version++),
                         &BB->front());
    ++NumPHIInsert;
    PhiToAllocaMap[PN] = AllocaNo;
    return true;
}

// 更新PHI节点流入位置
static void updateForIncomingValueLocation(PHINode *PN, DebugLoc DL,
                                           bool ApplyMergedLoc) {
    if (ApplyMergedLoc)
        PN->applyMergedLocation(PN->getDebugLoc(), DL);
    else
        PN->setDebugLoc(DL);
}

// 在空PHI节点中加入流入数据并处理替换掉load和store指令
void PromoteMem2Reg::RenamePass(BasicBlock *BB, BasicBlock *Pred,
                                RenamePassData::ValVector &IncomingVals,
                                RenamePassData::LocationVector &IncomingLocs,
                                std::vector<RenamePassData> &Worklist) {
    NextIteration:

    // 如果当前块插入了PHI节点
    if (PHINode *APN = dyn_cast<PHINode>(BB->begin())) {
        if (PhiToAllocaMap.count(APN)) {
            unsigned NewPHINumOperands = APN->getNumOperands();

            unsigned NumEdges = llvm::count(successors(Pred), BB);
            assert(NumEdges && "Must be at least one edge from Pred to BB!");

            BasicBlock::iterator PNI = BB->begin();
            do {
                unsigned AllocaNo = PhiToAllocaMap[APN];

                // 更新PHI节点流入位置
                updateForIncomingValueLocation(APN, IncomingLocs[AllocaNo],
                                               APN->getNumIncomingValues() > 0);

                // 为PHI节点添加操作数，流入数据来自于Pred块
                for (unsigned i = 0; i != NumEdges; ++i)
                    APN->addIncoming(IncomingVals[AllocaNo], Pred);

                // 记录在IncomingVals数组中
                IncomingVals[AllocaNo] = APN;

                // 继续处理下一个
                ++PNI;
                APN = dyn_cast<PHINode>(PNI);
                if (!APN)
                    break;

                // PHI节点可能源程序就有，为了区别源程序中的PHI节点和本pass添加的PHI节点，我们获取PHI节点的操作数
                // 手动添加的PHI节点的操作数一定相等，且初始值为0，即没有操作数。
            } while (APN->getNumOperands() == NewPHINumOperands);
        }
    }

    // 有环路的情况下可能重复处理，此时直接退出
    if (!Visited.insert(BB).second)
        return;

    // 遍历所有指令做替换。如果是store指令，更新原alloca值；如果是load指令，直接替换为alloca当前更新到的值。替换后删除原指令
    for (BasicBlock::iterator II = BB->begin(); !II->isTerminator();) {
        Instruction *I = &*II++;

        if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
            AllocaInst *Src = dyn_cast<AllocaInst>(LI->getPointerOperand());
            if (!Src)
                continue;

            DenseMap<AllocaInst *, unsigned>::iterator AI = AllocaLookup.find(Src);
            if (AI == AllocaLookup.end())
                continue;

            // // 找到load对应的alloca后，获取alloca在当前block的流入值
            Value *V = IncomingVals[AI->second];

            if (AC && LI->getMetadata(LLVMContext::MD_nonnull) &&
                !isKnownNonZero(V, SQ.DL, 0, AC, LI, &DT))
                addAssumeNonNull(AC, LI);

            // 用流入值代替所有user，移除load指令
            LI->replaceAllUsesWith(V);
            LI->eraseFromParent();
        } else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
            AllocaInst *Dest = dyn_cast<AllocaInst>(SI->getPointerOperand());
            if (!Dest)
                continue;

            DenseMap<AllocaInst *, unsigned>::iterator ai = AllocaLookup.find(Dest);
            if (ai == AllocaLookup.end())
                continue;

            // 获取store对应的alloca后，用store的参数来更新alloca的流出值
            unsigned AllocaNo = ai->second;
            IncomingVals[AllocaNo] = SI->getOperand(0);

            IncomingLocs[AllocaNo] = SI->getDebugLoc();
            // 移除store指令
            SI->eraseFromParent();
        }
    }

    // 如果没有后继可处理了，则退出
    succ_iterator I = succ_begin(BB), E = succ_end(BB);
    if (I == E)
        return;

    SmallPtrSet<BasicBlock *, 8> VisitedSuccs;

    // 第一个后继不放到队列里，而是直接在下一轮迭代中处理，对应DFS顺序
    VisitedSuccs.insert(*I);
    Pred = BB;
    BB = *I;
    ++I;

    // 其余后继则跟随当前block的流出值接入到worklist中。目的是实现IncomingVals的复用
    for (; I != E; ++I)
        if (VisitedSuccs.insert(*I).second)
            Worklist.emplace_back(*I, Pred, IncomingVals, IncomingLocs);

    goto NextIteration;
}

void llvm::PromoteMemToReg(ArrayRef<AllocaInst *> Allocas, DominatorTree &DT, AssumptionCache *AC) {
    PromoteMem2Reg(Allocas, DT, AC).run();
}
