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
        auto [inputFilename, outputFilename, optLevel] = cmdParse(argc, argv);

        // 输入重定向
        if (auto fd = freopen(inputFilename.c_str(), "r", stdin);
                fd == nullptr) {
            throw std::runtime_error("failed to open file: " + inputFilename);
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

        PassManager::run(optLevel, outputFilename);

    } catch (std::exception &e) {
        log("main") << "error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
