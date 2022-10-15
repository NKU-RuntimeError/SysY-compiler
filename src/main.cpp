#include <iostream>
#include <stdio.h>
#include "log.h"
#include "parser.h"

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

    return 0;
}
