#ifndef SYSY_COMPILER_FRONTEND_LEXER_H
#define SYSY_COMPILER_FRONTEND_LEXER_H

#include <optional>
#include <string>
#include <utility>

class Lexer {
    std::string rest;
public:
    explicit Lexer(std::string input) : rest(input) {}

    std::optional<int> getToken();
};

#endif //SYSY_COMPILER_FRONTEND_LEXER_H
