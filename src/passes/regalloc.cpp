#include <AllocationOrder.h>
#include <LiveDebugVariables.h>
#include <RegAllocBase.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/CodeGen/CalcSpillWeights.h>
#include <llvm/CodeGen/LiveIntervals.h>
#include <llvm/CodeGen/LiveRangeEdit.h>
#include <llvm/CodeGen/LiveRegMatrix.h>
#include <llvm/CodeGen/LiveStacks.h>
#include <llvm/CodeGen/MachineBlockFrequencyInfo.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/MachineLoopInfo.h>
#include <llvm/CodeGen/Passes.h>
#include <llvm/CodeGen/RegAllocRegistry.h>
#include <llvm/CodeGen/Spiller.h>
#include <llvm/CodeGen/TargetRegisterInfo.h>
#include <llvm/CodeGen/VirtRegMap.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include <queue>

using namespace llvm;

#define DEBUG_TYPE "regalloc"

static RegisterRegAlloc basicRegAlloc("basic", "basic register allocator", createBasicRegisterAllocator);

namespace {
    // 比较溢出权重的大小，用于判断物理寄存器分配顺序
    // 溢出权重是结合读写次数和基本块执行频率计算出的
    struct CompSpillWeight {
        bool operator()(LiveInterval *A, LiveInterval *B) const {
            return A->weight() < B->weight();
        }
    };
}

namespace {
    // 基本寄存器算法的实现，以溢出权重决定分配顺序，优先分配权重大的
    class RABasic : public MachineFunctionPass,
                    public RegAllocBase,
                    private LiveRangeEdit::Delegate {
        MachineFunction *MF;

        // 分配的顺序由优先队列决定
        std::unique_ptr<Spiller> SpillerInstance;
        std::priority_queue<LiveInterval *, std::vector<LiveInterval *>, CompSpillWeight> Queue;

        // 可用寄存器，在类内定义可防止在selectOrSplit函数中反复malloc
        BitVector UsableRegs;

        bool LRE_CanEraseVirtReg(Register) override;
        void LRE_WillShrinkVirtReg(Register) override;

    public:
        RABasic(const RegClassFilterFunc F = allocateAllRegClasses);

        StringRef getPassName() const override { return "Basic Register Allocator"; }

        // 向pass提供分析使用信息
        void getAnalysisUsage(AnalysisUsage &AU) const override;

        void releaseMemory() override;

        Spiller &spiller() override { return *SpillerInstance; }

        // 入队
        void enqueueImpl(LiveInterval *LI) override { Queue.push(LI); }

        // 出队，返回出队的LiveInterval
        LiveInterval *dequeue() override {
            if (Queue.empty())
                return nullptr;
            LiveInterval *LI = Queue.top();
            Queue.pop();
            return LI;
        }

        MCRegister selectOrSplit(LiveInterval &VirtReg, SmallVectorImpl<Register> &SplitVRegs) override;

        // 实现寄存器分配
        bool runOnMachineFunction(MachineFunction &mf) override;

        // 寄存器分配前的准备，包括PHI消除和二地址转换
        MachineFunctionProperties getRequiredProperties() const override {
            return MachineFunctionProperties().set(
                    MachineFunctionProperties::Property::NoPHIs);
        }

        MachineFunctionProperties getClearedProperties() const override {
            return MachineFunctionProperties().set(
                    MachineFunctionProperties::Property::IsSSA);
        }

        // 溢出虚拟寄存器的接口，溢出的虚拟寄存器会放入SplitVRegs中
        bool spillInterferences(LiveInterval &VirtReg, MCRegister PhysReg,
                                SmallVectorImpl<Register> &SplitVRegs);

        static char ID;
    };

    char RABasic::ID = 0;

}

char &llvm::RABasicID = RABasic::ID;

INITIALIZE_PASS_BEGIN(RABasic, "regallocbasic", "Basic Register Allocator",
                      false, false)
    INITIALIZE_PASS_DEPENDENCY(LiveDebugVariables)
    INITIALIZE_PASS_DEPENDENCY(SlotIndexes)
    INITIALIZE_PASS_DEPENDENCY(LiveIntervals)
    INITIALIZE_PASS_DEPENDENCY(RegisterCoalescer)
    INITIALIZE_PASS_DEPENDENCY(MachineScheduler)
    INITIALIZE_PASS_DEPENDENCY(LiveStacks)
    INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
    INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
    INITIALIZE_PASS_DEPENDENCY(VirtRegMap)
    INITIALIZE_PASS_DEPENDENCY(LiveRegMatrix)
INITIALIZE_PASS_END(RABasic, "regallocbasic", "Basic Register Allocator", false,
                    false)

// 当一个虚拟寄存器不会再被使用时，将其移除
bool RABasic::LRE_CanEraseVirtReg(Register VirtReg) {
    LiveInterval &LI = LIS->getInterval(VirtReg);
    // 如果该虚拟寄存器有分配物理寄存器，从物理寄存器中将其删除，表现为将物理寄存器对应的LiveInterval置为空闲
    if (VRM->hasPhys(VirtReg)) {
        Matrix->unassign(LI);
        aboutToRemoveInterval(LI);
        return true;
    }
    // 否则说明虚拟寄存器还在优先队列中等待分配/溢出了
    LI.clear();
    return false;
}

// 把已经分配物理寄存器了的虚拟寄存器收回入队，准备重新分配
void RABasic::LRE_WillShrinkVirtReg(Register VirtReg) {
    if (!VRM->hasPhys(VirtReg))
        return;
    LiveInterval &LI = LIS->getInterval(VirtReg);
    Matrix->unassign(LI);
    enqueue(&LI);
}

RABasic::RABasic(RegClassFilterFunc F):
        // 以Function为单位作为在机器指令上
        MachineFunctionPass(ID),
        RegAllocBase(F) {
}


void RABasic::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesCFG();
    AU.addRequired<AAResultsWrapperPass>();
    AU.addPreserved<AAResultsWrapperPass>();
    AU.addRequired<LiveIntervals>();
    AU.addPreserved<LiveIntervals>();
    AU.addPreserved<SlotIndexes>();
    AU.addRequired<LiveDebugVariables>();
    AU.addPreserved<LiveDebugVariables>();
    AU.addRequired<LiveStacks>();
    AU.addPreserved<LiveStacks>();
    AU.addRequired<MachineBlockFrequencyInfo>();
    AU.addPreserved<MachineBlockFrequencyInfo>();
    AU.addRequiredID(MachineDominatorsID);
    AU.addPreservedID(MachineDominatorsID);
    AU.addRequired<MachineLoopInfo>();
    AU.addPreserved<MachineLoopInfo>();
    AU.addRequired<VirtRegMap>();
    AU.addPreserved<VirtRegMap>();
    AU.addRequired<LiveRegMatrix>();
    AU.addPreserved<LiveRegMatrix>();
    MachineFunctionPass::getAnalysisUsage(AU);
}

// 释放内存
void RABasic::releaseMemory() {
    SpillerInstance.reset();
}

// 干涉检查，将PhysReg内与VirtReg的LiveInterval冲突的Live Range(Segment)找出
// 1：如果此LiveInterval比所有Live Range权重都大，溢出这些Live Range，保留LiveInterval
// 2：否则保留这些Live Range，溢出LiveInterval
// 基于优先队列，当前的LiveInterval必定比所有Live Range权重都小，只会发生后一种情况，不出错的情况下都返回false
bool RABasic::spillInterferences(LiveInterval &VirtReg, MCRegister PhysReg,
                                 SmallVectorImpl<Register> &SplitVRegs) {
    // 储存冲突的Live Range
    SmallVector<LiveInterval*, 8> Intfs;

    // 遍历PhysReg内的所有Live Range，寻找出与VirtReg有冲突的那些
    // 再遍历找出的这些Live Range，看看VirtReg的LiveInterval是不是比每一个Live Range溢出权重都大，有一个小于就返回false，即第2种情况
    // 否则把Live Range放入Intfs中，预备将它们溢出，即第1种情况
    for (MCRegUnitIterator Units(PhysReg, TRI); Units.isValid(); ++Units) {
        LiveIntervalUnion::Query &Q = Matrix->query(VirtReg, *Units);
        for (auto *Intf : reverse(Q.interferingVRegs())) {
            if (!Intf->isSpillable() || VirtReg.weight() < Intf->weight() )
                return false;
            Intfs.push_back(Intf);
        }
    }
    assert(!Intfs.empty() && "expected interference");

    // 接下来是第1种情况的处理，实际上是为线性扫描等无序分配算法设计的，这里用的优先队列自动有序，到不了这情况

    // 遍历Intfs中所有记录下的Live Range，将它们溢出
    for (unsigned i = 0, e = Intfs.size(); i != e; ++i) {
        LiveInterval &Spill = *Intfs[i];

        // 避免重复
        if (!VRM->hasPhys(Spill.reg()))
            continue;

        // 将Live Range从PhysReg中移除
        Matrix->unassign(Spill);

        // 溢出
        LiveRangeEdit LRE(&Spill, SplitVRegs, *MF, *LIS, VRM, this, &DeadRemats);
        spiller().spill(LRE);
    }
    return true;
}

// 选择分配给VirtReg的物理寄存器，如果VirtReg溢出则返回0
MCRegister RABasic::selectOrSplit(LiveInterval &VirtReg,
                                  SmallVectorImpl<Register> &SplitVRegs) {
    // 候选列表，记录与VirtReg冲突的物理寄存器
    SmallVector<MCRegister, 8> PhysRegSpillCands;

    // 得到VirtReg对应类的物理寄存器，即在无干涉冲突的情况下VirtReg所能分配的所有物理寄存器
    AllocationOrder Order = AllocationOrder::create(VirtReg.reg(), *VRM, RegClassInfo, Matrix);

    // 遍历这些物理寄存器，判断VirtReg的LiveInterval是否与PhysReg相交
    for (MCRegister PhysReg : Order) {
        assert(PhysReg.isValid());

        // 判断是否相交
        switch (Matrix->checkInterference(VirtReg, PhysReg)) {
            // 不相交，直接将VirtReg分配到这个PhysReg上
            case LiveRegMatrix::IK_Free:
                return PhysReg;
            // 相交，放入候选列表，等待进行干涉检查
            case LiveRegMatrix::IK_VirtReg:
                PhysRegSpillCands.push_back(PhysReg);
                continue;
            default:
                continue;
        }
    }

    // 遍历候选列表中所有物理寄存器，进行干涉检查
    for (MCRegister &PhysReg : PhysRegSpillCands) {
        // 在spillInterferences中我们知道由于优先队列的特性，会一直返回false，一直continue
        if (!spillInterferences(VirtReg, PhysReg, SplitVRegs))
            continue;

        assert(!Matrix->checkInterference(VirtReg, PhysReg) && "Interference after spill.");

        // 在线性扫描等无序分配的情况下，我们把PhysReg中原有的溢出权重小的LiveInterval清空并溢出，空出的地方分配给VirtReg
        return PhysReg;
    }

    // spillInterferences函数中第2中情况，溢出当前的VirtReg
    if (!VirtReg.isSpillable())
        return ~0u;
    LiveRangeEdit LRE(&VirtReg, SplitVRegs, *MF, *LIS, VRM, this, &DeadRemats);
    spiller().spill(LRE);

    // 溢出后也不需要分配物理寄存器了，直接返回0
    return 0;
}

// run函数，实际执行寄存器分配这项工作
bool RABasic::runOnMachineFunction(MachineFunction &mf) {

    MF = &mf;
    RegAllocBase::init(getAnalysis<VirtRegMap>(),
                       getAnalysis<LiveIntervals>(),
                       getAnalysis<LiveRegMatrix>());
    VirtRegAuxInfo VRAI(*MF, *LIS, *VRM, getAnalysis<MachineLoopInfo>(), getAnalysis<MachineBlockFrequencyInfo>());
    VRAI.calculateSpillWeightsAndHints();

    SpillerInstance.reset(createInlineSpiller(*this, *MF, *VRM, VRAI));

    // 在allocatePhysRegs中使用优先队列和selectOrSplit函数给虚拟寄存器分配物理寄存器
    allocatePhysRegs();
    postOptimization();

    // 分配完后释放溢出栈上的内存
    releaseMemory();
    return true;
}

FunctionPass* llvm::createBasicRegisterAllocator() {
    return new RABasic();
}

FunctionPass* llvm::createBasicRegisterAllocator(RegClassFilterFunc F) {
    return new RABasic(F);
}