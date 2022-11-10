#ifndef SYSY_COMPILER_FRONTEND_LOOP_INFO_H
#define SYSY_COMPILER_FRONTEND_LOOP_INFO_H

#include <llvm/IR/BasicBlock.h>

// 用于记录循环信息，在continue/break时知道应该跳转到哪里
struct LoopInfo {
    llvm::BasicBlock *continueBB;
    llvm::BasicBlock *breakBB;
};

#endif //SYSY_COMPILER_FRONTEND_LOOP_INFO_H
