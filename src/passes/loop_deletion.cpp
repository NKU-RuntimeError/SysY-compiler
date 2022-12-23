#include <loop_deletion.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/Statistic.h>
#include <llvm/Analysis/CFG.h>
#include <llvm/Analysis/GlobalsModRef.h>
#include <llvm/Analysis/InstructionSimplify.h>
#include <llvm/Analysis/LoopIterator.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/Analysis/MemorySSA.h>
#include <llvm/Analysis/OptimizationRemarkEmitter.h>
#include <llvm/IR/Dominators.h>

#include <llvm/IR/PatternMatch.h>
#include <llvm/InitializePasses.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/LoopPassManager.h>
#include <llvm/Transforms/Utils/LoopUtils.h>

using namespace llvm;

#define DEBUG_TYPE "loop-delete"

STATISTIC(NumDelete, "Number of loops deleted");
STATISTIC(NumBackedgesBroken,
          "Number of loops for which we managed to break the backedge");

static cl::opt<bool> EnableSymbolicExecution(
        "self-loop-deletion-enable-symbolic-execution", cl::Hidden, cl::init(true),
        cl::desc("Break backedge through symbolic execution of 1st iteration "
                 "attempting to prove that the backedge is never taken"));

// 三种状态：未修改、修改、删除
enum class LoopDeletionResult {
    Unmodified,
    Modified,
    Deleted,
};

// 取A和B中的较大者
static LoopDeletionResult merge(LoopDeletionResult A, LoopDeletionResult B) {
    if (A == LoopDeletionResult::Deleted || B == LoopDeletionResult::Deleted)
        return LoopDeletionResult::Deleted;
    if (A == LoopDeletionResult::Modified || B == LoopDeletionResult::Modified)
        return LoopDeletionResult::Modified;
    return LoopDeletionResult::Unmodified;
}

// 判断L是否为死循环。死循环要求流入exit块中PHI节点的都是循环不变量并且从不同exiting块流入的数据都相同,并且循环内部未与环境产生交互,并且不能是while(1)这类无法预估的无限循环
// 约束条件是exit只有一个，有exiting块，为LCSSA形式
// 传入的Changed是False
static bool isLoopDead(Loop *L, ScalarEvolution &SE,
                       SmallVectorImpl<BasicBlock *> &ExitingBlocks,
                       BasicBlock *ExitBlock, bool &Changed,
                       BasicBlock *Preheader, LoopInfo &LI) {
    // 死循环保证了流入PHI节点的数据都是循环不变量
    bool AllEntriesInvariant = true;
    // 并且从不同exiting块流入的数据都相同
    bool AllOutgoingValuesSame = true;
    if (!L->hasNoExitBlocks()) {
        // 遍历唯一的exit块中的所有PHI节点
        for (PHINode &P : ExitBlock->phis()) {
            // 取第一个exiting块流入的数据作为标准
            Value *incoming = P.getIncomingValueForBlock(ExitingBlocks[0]);

            // 判断是否不同exiting块流入PHI节点的数据都相同
            AllOutgoingValuesSame =
                    all_of(makeArrayRef(ExitingBlocks).slice(1), [&](BasicBlock *BB) {
                        return incoming == P.getIncomingValueForBlock(BB);
                    });

            // 非死循环，且不能把循环不变量外提
            // AllEntriesInvariant为True，Changed为False
            if (!AllOutgoingValuesSame)
                break;

            // 判断是否能将循环不变量外提
            // 如果能，AllEntriesInvariant为True，Changed为True
            if (Instruction *I = dyn_cast<Instruction>(incoming))
                if (!L->makeLoopInvariant(I, Changed, Preheader->getTerminator())) {
                    AllEntriesInvariant = false;
                    break;
                }
        }
    }

    // 发生了循环不变量外提。此时循环会被改变，也应在SCEV中遗忘
    if (Changed)
        SE.forgetLoopDispositions(L);

    // !AllEntriesInvariant说明不是死循环但可以把循环不变量外提
    // !AllOutgoingValuesSame说明不是死循环且不能把循环不变量外提
    if (!AllEntriesInvariant || !AllOutgoingValuesSame)
        return false;

    // 还有一种情况也非死循环(不可删除)：在循环内进行了写内存、使用volatile属性变量等操作
    for (auto &I : L->blocks())
        if (any_of(*I, [](Instruction &I) {
            return I.mayHaveSideEffects() && !I.isDroppable();
        }))
            return false;

    // 1.mustprogress说明Loop内没有进行I/O输出等与环境交互的操作，可以删除
    // 2.当Loop的子循环全都是mustprogress的或者有有限运行次数限制时，也看做死循环，可以删除
    // 第1种情况
    if (L->getHeader()->getParent()->mustProgress())
        return true;

    // 获取循环内所有Block形成的流程图
    LoopBlocksRPO RPOT(L);
    RPOT.perform(&LI);

    // 如果循环内存在着不能归约的环，循环会一直持续执行下去，此时认为需要的就是这种无限循环(如：while(1))，不能删除，记为非死循环
    if (containsIrreducibleCFG<const BasicBlock *>(RPOT, LI))
        return false;

    // 第2种情况，遍历Loop的子循环
    SmallVector<Loop *, 8> WorkList;
    WorkList.push_back(L);
    while (!WorkList.empty()) {
        Loop *Current = WorkList.pop_back_val();
        if (hasMustProgress(Current))
            continue;

        // 获取子循环的运行次数
        const SCEV *S = SE.getConstantMaxBackedgeTakenCount(Current);
        // 如果子循环的运行次数无法计算(无限)，并且不具有mustprogress属性(与环境产生了交互)，不能删除，记为非死循环
        if (isa<SCEVCouldNotCompute>(S)) {
            return false;
        }
        // 将子循环产生的子循环再放入WorkList中，一层一层检验
        WorkList.append(Current->begin(), Current->end());
    }
    // 所有非死循环的条件都未触发，记为死循环
    return true;
}

// 如果没有可进入L的入口块，说明L永不被运行，返回True
static bool isLoopNeverExecuted(Loop *L) {
    using namespace PatternMatch;

    // 首先保证Loop有前序块
    // 前序块Preheader的定义：Loop的唯一前继块，不发生任何跳转，运行后必定会进入Loop
    // 前继块predecessors的定义：不唯一，只要有分支到其后继块即可
    auto *Preheader = L->getLoopPreheader();
    assert(Preheader && "Needs preheader!");

    // 如果此前序块即为入口块(即程序第一个块)，Loop一定会被运行
    if (Preheader->isEntryBlock())
        return false;

    // 遍历前序块P的前继块PP，L不被运行要求PP无法走到P
    for (auto *Pred: predecessors(Preheader)) {
        BasicBlock *Taken, *NotTaken;
        ConstantInt *Cond;
        // 如果没有分支，PP会走到P
        if (!match(Pred->getTerminator(), m_Br(m_ConstantInt(Cond), Taken, NotTaken)))
            return false;
        // 这里默认PP走不到P(NotTaken)。如果有分支，且Cond的返回值非0，说明PP能走到P
        // 此时交换Taken和NotTaken两个块，P对应Taken，Loop会被运行
        if (!Cond->getZExtValue())
            std::swap(Taken, NotTaken);
        if (Taken == Preheader)
            return false;
    }
    assert(!pred_empty(Preheader) &&
           "Preheader should have predecessors at this point!");

    // 所有的PP都走不到P，Loop不会被运行
    return true;
}

// 获取从外界进入Loop的值V在第一次迭代完成后获得的值
static Value *
getValueOnFirstIteration(Value *V, DenseMap<Value *, Value *> &FirstIterValue,
                         const SimplifyQuery &SQ) {
    // 首先确认V为指令，如果非指令说明V并未被更改，直接返回V
    if (!isa<Instruction>(V))
        return V;

    // 在负责记录的Map中查找是否已经做过了V的记录，如果是，返回Map中记录的V的映射(即V在第一次迭代后的值)
    auto Existing = FirstIterValue.find(V);
    if (Existing != FirstIterValue.end())
        return Existing->second;

    // FirstIterV记录V在第一次迭代后的值
    Value *FirstIterV = nullptr;
    // 如果V指令是二元表达式，获得其两个操作数的值，计算后赋值给FirstIterV
    // 如果V是一个比较指令，同上
    // 如果V是一个条件判断式，计算出结果是TorF，递归调用本getValueOnFirstIteration函数，进入计算出的分支进一步求值
    if (auto *BO = dyn_cast<BinaryOperator>(V)) {
        Value *LHS =
                getValueOnFirstIteration(BO->getOperand(0), FirstIterValue, SQ);
        Value *RHS =
                getValueOnFirstIteration(BO->getOperand(1), FirstIterValue, SQ);
        FirstIterV = SimplifyBinOp(BO->getOpcode(), LHS, RHS, SQ);
    } else if (auto *Cmp = dyn_cast<ICmpInst>(V)) {
        Value *LHS =
                getValueOnFirstIteration(Cmp->getOperand(0), FirstIterValue, SQ);
        Value *RHS =
                getValueOnFirstIteration(Cmp->getOperand(1), FirstIterValue, SQ);
        FirstIterV = SimplifyICmpInst(Cmp->getPredicate(), LHS, RHS, SQ);
    } else if (auto *Select = dyn_cast<SelectInst>(V)) {
        Value *Cond =
                getValueOnFirstIteration(Select->getCondition(), FirstIterValue, SQ);
        if (auto *C = dyn_cast<ConstantInt>(Cond)) {
            auto *Selected = C->isAllOnesValue() ? Select->getTrueValue()
                                                 : Select->getFalseValue();
            FirstIterV = getValueOnFirstIteration(Selected, FirstIterValue, SQ);
        }
    }
    // 如果FirstIterV为空，说明V在第一次迭代中未变
    if (!FirstIterV)
        FirstIterV = V;
    // 将V和V第一次迭代后的值记录在映射FirstIterValue中
    FirstIterValue[V] = FirstIterV;
    return FirstIterV;
}

// 判断支配Latch块的条件是否会在第一次迭代后消失，决定Latch块是否能删除
static bool canProveExitOnFirstIteration(Loop *L, DominatorTree &DT,
                                         LoopInfo &LI) {
    // 是否开启了这一功能
    if (!EnableSymbolicExecution)
        return false;

    BasicBlock *Predecessor = L->getLoopPredecessor();
    BasicBlock *Latch = L->getLoopLatch();

    if (!Predecessor || !Latch)
        return false;

    // // 获取循环L内所有Block形成的流程图
    LoopBlocksRPO RPOT(L);
    RPOT.perform(&LI);

    // 优化的前提条件是L中任意一块必须要在它的所有前继块运行完毕后才能运行，即能拓扑排序
    // 循环L内有无法归约的环，则无法处理
    if (containsIrreducibleCFG<const BasicBlock *>(RPOT, LI))
        return false;

    // 记录下载第一次迭代时可达的块和边
    BasicBlock *Header = L->getHeader();
    SmallPtrSet<BasicBlock *, 4> LiveBlocks;
    DenseSet<BasicBlockEdge> LiveEdges;
    LiveBlocks.insert(Header);

    // 设计一个函数，实现可达块和边的插入
    SmallPtrSet<BasicBlock *, 4> Visited;
    auto MarkLiveEdge = [&](BasicBlock *From, BasicBlock *To) {
        assert(LiveBlocks.count(From) && "Must be live!");
        assert((LI.isLoopHeader(To) || !Visited.count(To)) &&
               "Only canonical backedges are allowed. Irreducible CFG?");
        assert((LiveBlocks.count(To) || !Visited.count(To)) &&
               "We already discarded this block as dead!");
        LiveBlocks.insert(To);
        LiveEdges.insert({ From, To });
    };

    // 对某个块的所有后继块，实现块和边的插入
    auto MarkAllSuccessorsLive = [&](BasicBlock *BB) {
        for (auto *Succ : successors(BB))
            MarkLiveEdge(BB, Succ);
    };

    // 检查是否所有前继块流入块BB的值都相同。如果相同返回该值的指针，不同则返回空指针
    auto GetSoleInputOnFirstIteration = [&](PHINode & PN)->Value * {
        BasicBlock *BB = PN.getParent();
        bool HasLivePreds = false;
        (void)HasLivePreds;
        // BB是循环第一个块的特殊情况
        if (BB == Header)
            return PN.getIncomingValueForBlock(Predecessor);
        // 遍历所有前继块
        Value *OnlyInput = nullptr;
        for (auto *Pred : predecessors(BB))
            if (LiveEdges.count({ Pred, BB })) {
                HasLivePreds = true;
                Value *Incoming = PN.getIncomingValueForBlock(Pred);
                // 如果流入值未确定，跳过该前继块
                if (isa<UndefValue>(Incoming))
                    continue;
                // 检查流入值是否相同
                if (OnlyInput && OnlyInput != Incoming)
                    return nullptr;
                OnlyInput = Incoming;
            }

        assert(HasLivePreds && "No live predecessors?");
        // 如果所有前继块流入值都未确定，就记流入值为未确定
        return OnlyInput ? OnlyInput : UndefValue::get(PN.getType());
    };
    DenseMap<Value *, Value *> FirstIterValue;

    auto &DL = Header->getModule()->getDataLayout();
    const SimplifyQuery SQ(DL);
    for (auto *BB : RPOT) {
        // 在Visited中放入循环L的所有块
        Visited.insert(BB);

        // 如果BB在第一次迭代中不可达，跳过它
        if (!LiveBlocks.count(BB))
            continue;

        // 如果循坏L嵌套了内层循环，BB在此内层循环中，插入BB后继块和BB到后继块的边，跳过它
        if (LI.getLoopFor(BB) != L) {
            MarkAllSuccessorsLive(BB);
            continue;
        }

        // 遍历BB的所有PHI节点。如果只有一个值Incoming流入该节点，记该节点在第一次迭代后的值为FirstIterV，并记录在FirstIterValue映射中
        for (auto &PN : BB->phis()) {
            if (!PN.getType()->isIntegerTy())
                continue;
            auto *Incoming = GetSoleInputOnFirstIteration(PN);
            if (Incoming && DT.dominates(Incoming, BB->getTerminator())) {
                Value *FirstIterV =
                        getValueOnFirstIteration(Incoming, FirstIterValue, SQ);
                FirstIterValue[&PN] = FirstIterV;
            }
        }

        // 以下执行实际的Backedge(Latch)删除处理
        // 1.如果我们能BB的某个特定后继块在第一次迭代中运行了，记录之，即MarkLiveEdge，相当于只保留下一个Latch
        // 2.如果不能证明，我们认为BB所有的后继块在第一次迭代中存活，即MarkAllSuccessorsLive
        using namespace PatternMatch;
        Value *Cond;
        BasicBlock *IfTrue, *IfFalse;
        auto *Term = BB->getTerminator();
        // 条件分支的情况
        if (match(Term, m_Br(m_Value(Cond),
                             m_BasicBlock(IfTrue), m_BasicBlock(IfFalse)))) {
            auto *ICmp = dyn_cast<ICmpInst>(Cond);
            // 如果未进行比较，BB会顺序往下流到达任意一个后续块，为情况2
            if (!ICmp || !ICmp->getType()->isIntegerTy()) {
                MarkAllSuccessorsLive(BB);
                continue;
            }

            // 如果第一次迭代前后的Icmp相同，说明Icmp实际上未被使用，记为情况2
            auto *KnownCondition = getValueOnFirstIteration(ICmp, FirstIterValue, SQ);
            if (KnownCondition == ICmp) {
                MarkAllSuccessorsLive(BB);
                continue;
            }
            if (isa<UndefValue>(KnownCondition)) {
                // 在比较结果未确定的情况下，如果L中即有True块也有False块，默认跳到True块，相当于情况1
                // 否则我们视为该Undef的值会被其他Transform pass处理，在这里先不标记后续块
                if (L->contains(IfTrue) && L->contains(IfFalse))
                    MarkLiveEdge(BB, IfTrue);
                continue;
            }
            auto *ConstCondition = dyn_cast<ConstantInt>(KnownCondition);
            if (!ConstCondition) {
                // 如果KnowCondition非不变量，显然不能进一步分析，为情况2
                MarkAllSuccessorsLive(BB);
                continue;
            }
            // 查看该不变量，大于0则进入True块，否则进入False块，为情况1
            if (ConstCondition->isAllOnesValue())
                MarkLiveEdge(BB, IfTrue);
            else
                MarkLiveEdge(BB, IfFalse);
        } else if (SwitchInst *SI = dyn_cast<SwitchInst>(Term)) {
            auto *SwitchValue = SI->getCondition();
            auto *SwitchValueOnFirstIter =
                    getValueOnFirstIteration(SwitchValue, FirstIterValue, SQ);
            auto *ConstSwitchValue = dyn_cast<ConstantInt>(SwitchValueOnFirstIter);
            // Switch指令同上，如果KnowCondition非不变量，显然不能进一步分析，为情况2
            if (!ConstSwitchValue) {
                MarkAllSuccessorsLive(BB);
                continue;
            }
            // 如果KnowCondition为不变量，Switch指令会流入某个固定的Case块中，为情况1
            auto CaseIterator = SI->findCaseValue(ConstSwitchValue);
            MarkLiveEdge(BB, CaseIterator->getCaseSuccessor());
        } else {
            // 如果指令为其他类型，无法确定BB会流入什么块，为情况2
            MarkAllSuccessorsLive(BB);
            continue;
        }
    }

    // 如果Latch被删除了，返回True
    return !LiveEdges.count({ Latch, Header });
}

// 将未使用过的backedge删除。当删除到只剩一条(只剩下一个Latch块)时满足了死循环的条件，可以进行删除
// 会影响Loop，但是根本的CFG等会保留
static LoopDeletionResult
breakBackedgeIfNotTaken(Loop *L, DominatorTree &DT, ScalarEvolution &SE,
                        LoopInfo &LI, MemorySSA *MSSA,
                        OptimizationRemarkEmitter &ORE) {
    assert(L->isLCSSAForm(DT) && "Expected LCSSA!");

    // 如果没Latch，说明不是Simplify形式，没法修改
    if (!L->getLoopLatch())
        return LoopDeletionResult::Unmodified;

    // 判断Loop循环中能否删去Latch块/backedge边
    auto *BTCMax = SE.getConstantMaxBackedgeTakenCount(L);
    if (!BTCMax->isZero()) {
        auto *BTC = SE.getBackedgeTakenCount(L);
        if (!BTC->isZero()) {
            // 如果该Latch块的Backedge边流经次数是无法预估的无限循环并且不为0(非死循环的条件)，多余的Latch块无法删除，不能修改
            if (!isa<SCEVCouldNotCompute>(BTC) && SE.isKnownNonZero(BTC))
                return LoopDeletionResult::Unmodified;
            // 如果Latch块的支配条件并没有在第一次迭代后消失，说明多余的Latch块无法删除，不能修改
            if (!canProveExitOnFirstIteration(L, DT, LI))
                return LoopDeletionResult::Unmodified;
        }
    }
    // 移除backedge，此时原本的死循环Loop经过修改已不再是死循环，相当于删除了
    ++NumBackedgesBroken;
    breakLoopBackedge(L, DT, SE, LI, MSSA);
    return LoopDeletionResult::Deleted;
    // 注意：此处的返回没有Modified，因为此处如果修改了直接就能过渡到Deleted，不需要Modified这个中间态
}

// 移除死循环
// 其实移除的不一定是死循环，如果该循环压根不会被运行/是死循环，都会被删除
// 要求循环是Simplify+LCSSA的形式，即只有一个preheader block、只有一个Latch、只有一条backedge(Simplify)、exit块内有PHI节点(LCSSA)
// 另外还要求只有一个exit块
// 如果循环被删除，返回Deleted；如果出现不变量外提的操作返回Modified，否则返回Unmodified
static LoopDeletionResult deleteLoopIfDead(Loop *L, DominatorTree &DT,
                                           ScalarEvolution &SE, LoopInfo &LI,
                                           MemorySSA *MSSA,
                                           OptimizationRemarkEmitter &ORE) {
    assert(L->isLCSSAForm(DT) && "Expected LCSSA!");

    // 要求Loop必须有一个前序基本块，并且只有一个Exit(满足Simplify模式)
    BasicBlock *Preheader = L->getLoopPreheader();
    if (!Preheader || !L->hasDedicatedExits()) {
        return LoopDeletionResult::Unmodified;
    }

    BasicBlock *ExitBlock = L->getUniqueExitBlock();

    // 如果只有一个Exit并且该循环从未运行过，可以删除之
    if (ExitBlock && isLoopNeverExecuted(L)) {
        // 此时L会被修改，所以先删除SCEV中原本记载的L的信息
        SE.forgetLoop(L);

        // 把Exit块中的PHI节点流入数据都设为undef
        for (PHINode &P : ExitBlock->phis()) {
            std::fill(P.incoming_values().begin(), P.incoming_values().end(),
                      UndefValue::get(P.getType()));
        }
        ORE.emit([&]() {
            return OptimizationRemark(DEBUG_TYPE, "NeverExecutes", L->getStartLoc(),
                                      L->getHeader())
                    << "Loop deleted because it never executes";
        });
        // 删除循环本体L
        deleteDeadLoop(L, &DT, &SE, &LI, MSSA);
        ++NumDelete;
        return LoopDeletionResult::Deleted;
    }

    // 如果循环外所有变量经过循环都没被改变，也能删除
    SmallVector<BasicBlock *, 4> ExitingBlocks;
    L->getExitingBlocks(ExitingBlocks);

    // 如果有Exit块但不止一个，这种情况下我们无法确定Loop结束后会到哪个块里
    // 所以Loop无法删除
    if (!ExitBlock && !L->hasNoExitBlocks()) {
        return LoopDeletionResult::Unmodified;
    }

    // 如果Loop并非死循环(没被删除)，判断它是否被修改过(不变量外提)，根据传回来的Changed判断
    bool Changed = false;
    if (!isLoopDead(L, SE, ExitingBlocks, ExitBlock, Changed, Preheader, LI)) {
        LLVM_DEBUG(dbgs() << "Loop is not invariant, cannot delete.\n");
        return Changed ? LoopDeletionResult::Modified
                       : LoopDeletionResult::Unmodified;
    }

    // 否则说明已判断出Loop为死循环，也可将其删除
    ORE.emit([&]() {
        return OptimizationRemark(DEBUG_TYPE, "Invariant", L->getStartLoc(),
                                  L->getHeader())
                << "Loop deleted because it is invariant";
    });
    deleteDeadLoop(L, &DT, &SE, &LI, MSSA);
    ++NumDelete;

    return LoopDeletionResult::Deleted;
}

// 新版Pass的run函数
PreservedAnalyses LoopDeletionPass::run(Loop &L, LoopAnalysisManager &AM,
                                        LoopStandardAnalysisResults &AR,
                                        LPMUpdater &Updater) {

    std::string LoopName = std::string(L.getName());

    // 执行死循环删除
    OptimizationRemarkEmitter ORE(L.getHeader()->getParent());
    auto Result = deleteLoopIfDead(&L, AR.DT, AR.SE, AR.LI, AR.MSSA, ORE);

    // 如果没找到死循环删除，执行一下breakBackedgeIfNotTaken，该函数可能产生新的死循环并删除(但会保留原Loop结构)。最终的Result状态取Result和该函数状态的较大值
    if (Result != LoopDeletionResult::Deleted)
        Result = merge(Result, breakBackedgeIfNotTaken(&L, AR.DT, AR.SE, AR.LI,
                                                       AR.MSSA, ORE));
    // 返回不同的Result对应的分析结果
    if (Result == LoopDeletionResult::Unmodified)
        return PreservedAnalyses::all();

    if (Result == LoopDeletionResult::Deleted)
        Updater.markLoopAsDeleted(L, LoopName);

    auto PA = getLoopPassPreservedAnalyses();
    if (AR.MSSA)
        PA.preserve<MemorySSAAnalysis>();
    return PA;
}

namespace {
    class LoopDeletionLegacyPass : public LoopPass {
    public:
        static char ID;
        LoopDeletionLegacyPass() : LoopPass(ID) {
            initializeLoopDeletionLegacyPassPass(*PassRegistry::getPassRegistry());
        }

        bool runOnLoop(Loop *L, LPPassManager &) override;

        void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.addPreserved<MemorySSAWrapperPass>();
            getLoopAnalysisUsage(AU);
        }
    };
}

char LoopDeletionLegacyPass::ID = 0;
INITIALIZE_PASS_BEGIN(LoopDeletionLegacyPass, "loop-deletion",
                      "Delete dead loops", false, false)
    INITIALIZE_PASS_DEPENDENCY(LoopPass)
INITIALIZE_PASS_END(LoopDeletionLegacyPass, "loop-deletion",
                    "Delete dead loops", false, false)

Pass *llvm::createLoopDeletionPass() { return new LoopDeletionLegacyPass(); }

// 旧版Pass的run函数
bool LoopDeletionLegacyPass::runOnLoop(Loop *L, LPPassManager &LPM) {
    if (skipLoop(L))
        return false;
    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    auto *MSSAAnalysis = getAnalysisIfAvailable<MemorySSAWrapperPass>();
    MemorySSA *MSSA = nullptr;
    if (MSSAAnalysis)
        MSSA = &MSSAAnalysis->getMSSA();
    OptimizationRemarkEmitter ORE(L->getHeader()->getParent());

    LoopDeletionResult Result = deleteLoopIfDead(L, DT, SE, LI, MSSA, ORE);

    if (Result != LoopDeletionResult::Deleted)
        Result = merge(Result, breakBackedgeIfNotTaken(L, DT, SE, LI, MSSA, ORE));

    if (Result == LoopDeletionResult::Deleted)
        LPM.markLoopAsDeleted(*L);

    return Result != LoopDeletionResult::Unmodified;
}