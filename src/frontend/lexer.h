#ifndef SYSY_COMPILER_FRONTEND_LEXER_H
#define SYSY_COMPILER_FRONTEND_LEXER_H

#include <optional>
#include <string>
#include <utility>
#include <regex>

class Lexer {
    std::string input;
    std::sregex_iterator it, end;
    std::regex regex;

    static void changeRowCol(const std::string &str, size_t &row, size_t &col);

public:
    explicit Lexer(std::string input) : input(std::move(input)) {}

    std::optional<int> getToken();
};

#endif //SYSY_COMPILER_FRONTEND_LEXER_H
