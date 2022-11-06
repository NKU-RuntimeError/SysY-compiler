#include <iostream>
#include <cstdio>
#include "AST.h"
#include "log.h"
#include "parser.h"
#include "mem.h"
#include "IR.h"
#include "pass_manager.h"

int main(int argc, char *argv[]) {
    log("main") << "SysY compiler" << std::endl;

    // 将标准输入重定向到给定文件
    if (argc >= 2) {
        log("main") << "read from file: " << argv[1] << std::endl;
        freopen(argv[1], "r", stdin);
    } else {
        log("main") << "using standard input" << std::endl;
    }

    yyparse();

    log("main") << "final AST root: " << AST::root << std::endl;

    // 打印AST
    // 在long_code2 testcase中会出现树过大的情况，导致打印出来的AST过长，因此暂时不打印树结构
    AST::show();

    AST::root->constEval(AST::root);

    AST::show();

    AST::root->codeGen();

    // 释放AST占用的内存
    Memory::freeAll();

    IR::show();

    PassManager::run(llvm::OptimizationLevel::O3);

    IR::show();

    return 0;
}
