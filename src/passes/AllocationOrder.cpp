#include "AllocationOrder.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterClassInfo.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "regalloc"

// Compare VirtRegMap::getRegAllocPref().
AllocationOrder AllocationOrder::create(unsigned VirtReg, const VirtRegMap &VRM,
                                        const RegisterClassInfo &RegClassInfo,
                                        const LiveRegMatrix *Matrix) {
    const MachineFunction &MF = VRM.getMachineFunction();
    const TargetRegisterInfo *TRI = &VRM.getTargetRegInfo();
    auto Order = RegClassInfo.getOrder(MF.getRegInfo().getRegClass(VirtReg));
    SmallVector<MCPhysReg, 16> Hints;
    bool HardHints =
            TRI->getRegAllocationHints(VirtReg, Order, Hints, MF, &VRM, Matrix);

    LLVM_DEBUG({
                   if (!Hints.empty()) {
                       dbgs() << "hints:";
                       for (unsigned I = 0, E = Hints.size(); I != E; ++I)
                           dbgs() << ' ' << printReg(Hints[I], TRI);
                       dbgs() << '\n';
                   }
               });
#ifndef NDEBUG
    for (unsigned I = 0, E = Hints.size(); I != E; ++I)
        assert(is_contained(Order, Hints[I]) &&
               "Target hint is outside allocation order.");
#endif
    return AllocationOrder(std::move(Hints), Order, HardHints);
}