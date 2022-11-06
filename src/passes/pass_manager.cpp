#include <llvm/Passes/PassBuilder.h>
#include "IR.h"
#include "hello_world_pass.h"
#include "pass_manager.h"

// 使用llvm的新pass manager
// https://llvm.org/docs/NewPassManager.html
void PassManager::run(llvm::OptimizationLevel level) {

    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    llvm::PassBuilder PB;

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    // 在优化管道前端加入自己的pass
    PB.registerPipelineStartEPCallback(
            [&](llvm::ModulePassManager &MPM, llvm::OptimizationLevel level) {
                MPM.addPass(llvm::createModuleToFunctionPassAdaptor(HelloWorldPass()));
            }
    );

    llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(level);

    MPM.run(IR::ctx.module, MAM);
}
