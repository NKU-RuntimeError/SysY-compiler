#include <optional>
#include <string>
#include <regex>
#include <mutex>
#include <iomanip>
#include <utility>
#include "log.h"
#include "mem.h"
#include "position.h"
#include "lexer.h"

// 由于我们需要从语法分析器的头文件中得到所有token值
// 所以我们需要在这里include这个头文件
// 虽然会出现循环引用，但是不影响编译过程
#include "parser.h"

struct Pattern {
    const std::string regex;
    const std::function<std::optional<int>(std::string)> callback;

    static std::string fixGroup(const std::string &pattern);

    Pattern(
            const std::string &pattern,
            std::function<std::optional<int>(std::string)> callback
    );
};

// 查找前面不是斜杠的括号，替换成非捕获组
std::string Pattern::fixGroup(const std::string &pattern) {
    std::string dummyPattern = "Z" + pattern;
    std::string fixedPattern;
    fixedPattern.reserve(pattern.length());
    for (int i = 1; i < dummyPattern.length(); i++) {
        if (dummyPattern[i] == '(' && dummyPattern[i - 1] != '\\') {
            fixedPattern += "(?:";
        } else {
            fixedPattern += dummyPattern[i];
        }
    }
    return fixedPattern;
}

Pattern::Pattern(const std::string &pattern,
                 std::function<std::optional<int>(std::string)> callback)
        : regex(fixGroup(pattern)), callback(std::move(callback)) {}

// 记录行号，列号
static size_t currRow = 1;
static size_t currCol = 1;

// 包含patterns数组
// getToken函数通过从上到下遍历patterns数组，匹配第一个匹配的pattern
// 然后调用其对应的callback，获得token的类型
#include "lexer_pattern.inc"

void Lexer::changeRowCol(const std::string &str, size_t &row, size_t &col) {
    // 计算新行号
    size_t newLineCount = std::count_if(str.begin(), str.end(),
                                        [](char c) { return c == '\n'; });
    row += newLineCount;

    // 计算新列号
    if (size_t lastNewLinePos = str.find_last_of('\n'); lastNewLinePos != std::string::npos) {
        col = str.length() - lastNewLinePos;
    } else {
        col += str.length();
    }
}

// https://stackoverflow.com/questions/34229328/writing-a-very-simple-lexical-analyser-in-c
std::optional<int> Lexer::getToken() {

    static std::once_flag onceFlag;

    std::call_once(onceFlag, [this] {
        // 按序合并所有正则表达式
        std::string regexMerge;
        for (const auto &pattern: patterns) {
            regexMerge += "(" + pattern.regex + ")|";
        }
        // 去除最后一个竖线
        regexMerge.pop_back();
        // 编译正则表达式
        regex = std::regex(regexMerge);
        it = std::sregex_iterator(input.begin(), input.end(), regex);
        end = std::sregex_iterator();
    });

    retry:
    if (it == end) {
        return std::nullopt;
    }

    for (int i = 0; i < it->size(); i++) {
        if ((*it)[i + 1].matched) {
            std::string str = (*it)[i + 1].str();
            std::optional<int> token = patterns[i].callback(str);
            changeRowCol(str, currRow, currCol);
            it++;
            if (token) {
                return token;
            }
            goto retry;
        }
    }

    // 无法匹配任何pattern
    return std::nullopt;
}

int yylex() {
    static std::string s(std::istreambuf_iterator(std::cin), {});
    static Lexer lexer{s};

    // 在开始词法分析时调用一次，打印表头
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        log("lexer") <<
                     std::setw(20) << "token" <<
                     std::setw(20) << "lexeme" <<
                     std::setw(10) << "line" <<
                     std::setw(10) << "column" <<
                     std::endl;
    });

    if (auto token = lexer.getToken()) {
        return *token;
    }
    return 0;
}
