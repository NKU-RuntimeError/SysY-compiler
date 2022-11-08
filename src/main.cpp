#include <iostream>
#include <tuple>
#include <string>
#include <cstdio>
#include "AST.h"
#include "log.h"
#include "parser.h"
#include "mem.h"
#include "IR.h"
#include "pass_manager.h"
#include "scope.h"

// 命令行格式：
// compiler -S -o testcase.s testcase.sy
// compiler -S -o testcase.s testcase.sy -O2
// [0]      [1][2][3]        [4]         [5]

static std::tuple<std::string, std::string, int>
cmdParse(int argc, char *argv[]) {
    if (argc != 5 && argc != 6) {
        throw std::runtime_error("invalid command params");
    }

    // 获得优化级别
    int optLevel = 0;
    if (argc == 6 && std::string_view(argv[5]) == "-O2") {
        optLevel = 2;
    }
    return {argv[4], argv[3], optLevel};
}

int main(int argc, char *argv[]) {
    log("main") << "SysY compiler" << std::endl;

    try {
        // 在try块结束后自动释放内存
        // 由于使用的是C++17，还没有scope_exit特性，所以用了个非标准的实现
        nonstd::scope_exit cleanup([] {
            log("main") << "clean up" << std::endl;
            Memory::freeAll();
        });

        // 解析命令行参数
        auto [inputFilename, outputFilename, optLevel] = cmdParse(argc, argv);

        // 输入重定向
        if (auto fd = freopen(inputFilename.c_str(), "r", stdin);
                fd == nullptr) {
            throw std::runtime_error("failed to open file: " + inputFilename);
        }

        // 生成AST
        yyparse();

        log("main") << "AST root at: " << AST::root << std::endl;

        // 打印原始AST
        AST::show();

        // 常量求值，包括：常量初值、全局变量初值、数组维度
        AST::root->constEval(AST::root);

        // 展示常量求值后的AST
        AST::show();

        // IR生成
        AST::root->codeGen();

        // 在运行Pass前释放AST占用的内存，降低内存占用峰值
        Memory::freeAll();

        // 展示原始IR
        IR::show();

        // 生成汇编代码
        PassManager::run(optLevel, outputFilename);

    } catch (std::exception &e) {
        err("main") << "error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
