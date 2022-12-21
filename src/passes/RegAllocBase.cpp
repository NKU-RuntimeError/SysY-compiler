#include "RegAllocBase.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/LiveInterval.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LiveRegMatrix.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Spiller.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>

using namespace llvm;

#define DEBUG_TYPE "regalloc"

STATISTIC(NumNewQueued, "Number of new live ranges queued");

// Temporary verification option until we can put verification inside
// MachineVerifier.
static cl::opt<bool, true>
        VerifyRegAlloc("verify-regalloc", cl::location(RegAllocBase::VerifyEnabled),
                       cl::Hidden, cl::desc("Verify during register allocation"));

const char RegAllocBase::TimerGroupName[] = "regalloc";
const char RegAllocBase::TimerGroupDescription[] = "Register Allocation";
bool RegAllocBase::VerifyEnabled = false;

//===----------------------------------------------------------------------===//
//                         RegAllocBase Implementation
//===----------------------------------------------------------------------===//

// Pin the vtable to this file.
void RegAllocBase::anchor() {}

void RegAllocBase::init(VirtRegMap &vrm, LiveIntervals &lis,
                        LiveRegMatrix &mat) {
    TRI = &vrm.getTargetRegInfo();
    MRI = &vrm.getRegInfo();
    VRM = &vrm;
    LIS = &lis;
    Matrix = &mat;
    MRI->freezeReservedRegs(vrm.getMachineFunction());
    RegClassInfo.runOnMachineFunction(vrm.getMachineFunction());
}

// 遍历当前存活的所有虚拟寄存器，将这些未分配的物理寄存器放入优先队列中等待分配
void RegAllocBase::seedLiveRegs() {
    NamedRegionTimer T("seed", "Seed Live Regs", TimerGroupName,
                       TimerGroupDescription, TimePassesIsEnabled);
    for (unsigned i = 0, e = MRI->getNumVirtRegs(); i != e; ++i) {
        Register Reg = Register::index2VirtReg(i);
        // 该虚拟寄存器除了定义的debug之外并没有实际使用，则无需分配
        if (MRI->reg_nodbg_empty(Reg))
            continue;
        enqueue(&LIS->getInterval(Reg));
    }
}

// 负责管理未分配的虚拟寄存器的顶层驱动
void RegAllocBase::allocatePhysRegs() {
    // 把存活的未分配虚拟寄存器放入优先队列
    seedLiveRegs();

    // 遍历所有的未分配虚拟寄存器
    while (LiveInterval *VirtReg = dequeue()) {
        assert(!VRM->hasPhys(VirtReg->reg()) && "Register already assigned");

        // 在代码段合并的过程中可能又出现未使用过的虚拟寄存器，则将它们移除
        if (MRI->reg_nodbg_empty(VirtReg->reg())) {
            aboutToRemoveInterval(*VirtReg);
            LIS->removeInterval(VirtReg->reg());
            continue;
        }

        // 修改虚拟寄存器Live Range后需使干涉查询无效，避免查询到旧信息。直到新信息缓存完毕
        Matrix->invalidateVirtRegs();

        // 调用regalloc.cpp中的selectOrSplit函数查询可分配给当前虚拟寄存器的物理寄存器
        using VirtRegVec = SmallVector<Register, 4>;

        VirtRegVec SplitVRegs;
        MCRegister AvailablePhysReg = selectOrSplit(*VirtReg, SplitVRegs);

        // 未找到可分配的物理寄存器，可能是由于内联了汇编代码导致的，此时会导致需要更多的寄存器来进行分配
        if (AvailablePhysReg == ~0u) {
            MachineInstr *MI = nullptr;
            for (MachineRegisterInfo::reg_instr_iterator
                         I = MRI->reg_instr_begin(VirtReg->reg()),
                         E = MRI->reg_instr_end();
                         I != E;) {
                MI = &*(I++);
                if (MI->isInlineAsm())
                    break;
            }

            // 输出报错信息，包括无可用寄存器、内联汇编、所有寄存器用完了等
            const TargetRegisterClass *RC = MRI->getRegClass(VirtReg->reg());
            ArrayRef<MCPhysReg> AllocOrder = RegClassInfo.getOrder(RC);
            if (AllocOrder.empty())
                report_fatal_error("no registers from class available to allocate");
            else if (MI && MI->isInlineAsm()) {
                MI->emitError("inline assembly requires more registers than available");
            } else if (MI) {
                LLVMContext &Context =
                        MI->getParent()->getParent()->getMMI().getModule()->getContext();
                Context.emitError("ran out of registers during register allocation");
            } else {
                report_fatal_error("ran out of registers during register allocation");
            }

            VRM->assignVirt2Phys(VirtReg->reg(), AllocOrder.front());
            continue;
        }

        // 如果有可分配的物理寄存器就直接分配
        if (AvailablePhysReg)
            Matrix->assign(*VirtReg, AvailablePhysReg);

        // 对于溢出的寄存器，将它们按LiveInterval分割成小块，重新放入队列中
        for (Register Reg : SplitVRegs) {
            assert(LIS->hasInterval(Reg));

            LiveInterval *SplitVirtReg = &LIS->getInterval(Reg);
            assert(!VRM->hasPhys(SplitVirtReg->reg()) && "Register already assigned");
            // 新分割出的寄存器中，和上文一样，未使用过的寄存器不用分配，直接移除
            if (MRI->reg_nodbg_empty(SplitVirtReg->reg())) {
                assert(SplitVirtReg->empty() && "Non-empty but used interval");
                aboutToRemoveInterval(*SplitVirtReg);
                LIS->removeInterval(SplitVirtReg->reg());
                continue;
            }
            assert(Register::isVirtualRegister(SplitVirtReg->reg()) &&
                   "expect split value in virtual register");
            // 其余的新分割出的寄存器入队，等待下一次分配
            enqueue(SplitVirtReg);
            ++NumNewQueued;
        }
    }
}

void RegAllocBase::postOptimization() {
    spiller().postOptimization();
    for (auto DeadInst : DeadRemats) {
        LIS->RemoveMachineInstrFromMaps(*DeadInst);
        DeadInst->eraseFromParent();
    }
    DeadRemats.clear();
}

void RegAllocBase::enqueue(LiveInterval *LI) {
    const Register Reg = LI->reg();

    assert(Reg.isVirtual() && "Can only enqueue virtual registers");

    if (VRM->hasPhys(Reg))
        return;

    const TargetRegisterClass &RC = *MRI->getRegClass(Reg);
    if (ShouldAllocateClass(*TRI, RC)) {
        LLVM_DEBUG(dbgs() << "Enqueuing " << printReg(Reg, TRI) << '\n');
        enqueueImpl(LI);
    } else {
        LLVM_DEBUG(dbgs() << "Not enqueueing " << printReg(Reg, TRI)
                          << " in skipped register class\n");
    }
}