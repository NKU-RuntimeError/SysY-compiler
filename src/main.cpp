#include <iostream>
#include "lexer.h"

extern int chars;
extern int lines;

int main() {
    yylex();

    std::cout << chars << std::endl;
    std::cout << lines << std::endl;

    return 0;
}
