#include <llvm/Passes/PassBuilder.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
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

    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    std::string err;
    auto triple = "armv7-unknown-linux-gnu";
    auto target = llvm::TargetRegistry::lookupTarget(triple, err);
    if (!target) {
        throw std::logic_error(err);
    }

    // only for raspberry pi 4b
    auto CPU = "cortex-a72";
    auto features = "";
    auto targetMachine =
            target->createTargetMachine(triple, CPU, features, {}, {});

    IR::ctx.module.setDataLayout(targetMachine->createDataLayout());
    IR::ctx.module.setTargetTriple(triple);

    llvm::PassBuilder PB(targetMachine);

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
