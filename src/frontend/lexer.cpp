#include <optional>
#include <string>
#include <regex>
#include <mutex>
#include <iomanip>
#include "log.h"
#include "symbol_table.h"
#include "lexer.h"

// 由于我们需要从语法分析器的头文件中得到所有token值
// 所以我们需要在这里include这个头文件
// 虽然会出现循环引用，但是不影响编译过程
#include "parser.h"

struct Pattern {
    const std::regex regex;
    const std::function<std::optional<int>(std::string)> callback;

    Pattern(
            std::string pattern,
            std::function<std::optional<int>(std::string)> callback
    );
};

// 负责拼接一下字符串，其中^用于匹配开头
inline static std::regex getRegex(const std::string &regex) {
    return std::regex{"^(" + regex + ")"};
}

Pattern::Pattern(std::string pattern, std::function<std::optional<int>(std::string)> callback)
        : regex(getRegex(pattern)), callback(callback) {}

// 记录行号，列号
static size_t currLine = 1;
static size_t currCol = 1;

// 包含patterns数组
// getToken函数通过从上到下遍历patterns数组，匹配第一个匹配的pattern
// 然后调用其对应的callback，获得token的类型
#include "lexer_pattern.inc"

std::optional<int> Lexer::getToken() {
    retry:
    // 遍历所有规则，找到第一个匹配的规则
    for (const auto &pattern: patterns) {
        std::smatch match;
        if (std::regex_search(rest, match, pattern.regex)) {

            // 由于match只是一个引用，因此我们需要将匹配结果复制出来
            // 然后在调用的时候传递给callback
            const std::string &matchStr = match.str(0);
            std::string copy{matchStr.c_str(), matchStr.length()};
            rest = match.suffix();
            if (auto token = pattern.callback(copy)) {
                return token;
            }

            // 当前的token已经被吃掉了，因此回到开头重新匹配即可
            goto retry;
        }
    }

    // 如果没有匹配到任何规则，那么就返回空，代表结束。
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
    return YYEOF;
}
