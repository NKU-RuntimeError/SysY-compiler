#ifndef LLVM_LIB_CODEGEN_REGALLOCBASE_H
#define LLVM_LIB_CODEGEN_REGALLOCBASE_H

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/CodeGen/RegAllocCommon.h"
#include "llvm/CodeGen/RegisterClassInfo.h"

namespace llvm {

    class LiveInterval;
    class LiveIntervals;
    class LiveRegMatrix;
    class MachineInstr;
    class MachineRegisterInfo;
    template<typename T> class SmallVectorImpl;
    class Spiller;
    class TargetRegisterInfo;
    class VirtRegMap;

/// RegAllocBase provides the register allocation driver and interface that can
/// be extended to add interesting heuristics.
///
/// Register allocators must override the selectOrSplit() method to implement
/// live range splitting. They must also override enqueue/dequeue to provide an
/// assignment order.
    class RegAllocBase {
        virtual void anchor();

    protected:
        const TargetRegisterInfo *TRI = nullptr;
        MachineRegisterInfo *MRI = nullptr;
        VirtRegMap *VRM = nullptr;
        LiveIntervals *LIS = nullptr;
        LiveRegMatrix *Matrix = nullptr;
        RegisterClassInfo RegClassInfo;
        const RegClassFilterFunc ShouldAllocateClass;

        /// Inst which is a def of an original reg and whose defs are already all
        /// dead after remat is saved in DeadRemats. The deletion of such inst is
        /// postponed till all the allocations are done, so its remat expr is
        /// always available for the remat of all the siblings of the original reg.
        SmallPtrSet<MachineInstr *, 32> DeadRemats;

        RegAllocBase(const RegClassFilterFunc F = allocateAllRegClasses) :
                ShouldAllocateClass(F) {}

        virtual ~RegAllocBase() = default;

        // A RegAlloc pass should call this before allocatePhysRegs.
        void init(VirtRegMap &vrm, LiveIntervals &lis, LiveRegMatrix &mat);

        // The top-level driver. The output is a VirtRegMap that us updated with
        // physical register assignments.
        void allocatePhysRegs();

        // Include spiller post optimization and removing dead defs left because of
        // rematerialization.
        virtual void postOptimization();

        // Get a temporary reference to a Spiller instance.
        virtual Spiller &spiller() = 0;

        /// enqueue - Add VirtReg to the priority queue of unassigned registers.
        virtual void enqueueImpl(LiveInterval *LI) = 0;

        /// enqueue - Add VirtReg to the priority queue of unassigned registers.
        void enqueue(LiveInterval *LI);

        /// dequeue - Return the next unassigned register, or NULL.
        virtual LiveInterval *dequeue() = 0;

        // A RegAlloc pass should override this to provide the allocation heuristics.
        // Each call must guarantee forward progess by returning an available PhysReg
        // or new set of split live virtual registers. It is up to the splitter to
        // converge quickly toward fully spilled live ranges.
        virtual MCRegister selectOrSplit(LiveInterval &VirtReg,
                                         SmallVectorImpl<Register> &splitLVRs) = 0;

        // Use this group name for NamedRegionTimer.
        static const char TimerGroupName[];
        static const char TimerGroupDescription[];

        /// Method called when the allocator is about to remove a LiveInterval.
        virtual void aboutToRemoveInterval(LiveInterval &LI) {}

    public:
        /// VerifyEnabled - True when -verify-regalloc is given.
        static bool VerifyEnabled;

    private:
        void seedLiveRegs();
    };

} // end namespace llvm

#endif // LLVM_LIB_CODEGEN_REGALLOCBASE_H