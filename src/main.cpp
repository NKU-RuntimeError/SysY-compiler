#include <iostream>
#include "log.h"
#include "lexer.h"

int main() {
    log("main") << "SysY compiler" << std::endl;

    yyparse();

    return 0;
}
