#ifndef SYSY_COMPILER_FRONTEND_SYMBOL_TABLE_H
#define SYSY_COMPILER_FRONTEND_SYMBOL_TABLE_H

#include <list>
#include <map>
#include <string>
#include <llvm/IR/Value.h>
#include "log.h"

template <typename Ty>
class SymbolTable {

    // 为什么在此使用llvm::Value*裸指针：
    // 简单的来说，llvm::Value的生命周期受llvm模块管理，我们使用的指针类似于weak_ptr
    //
    // https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl03.html
    //
    // "TheModule is an LLVM construct that contains functions and global variables.
    // In many ways, it is the top-level structure that the LLVM IR uses to contain code.
    // It will own the memory for all of the IR that we generate, which is why the codegen()
    // method returns a raw Value*, rather than a unique_ptr<Value>."

    std::list<std::map<std::string, Ty>> symbolStack;
public:
    // 构造函数，负责创建一个全局符号表
    SymbolTable() {
        log("sym_table") << "new symbol table" << std::endl;

        // 创建一个空的符号表作为全局符号表
        symbolStack.emplace_back();
    }

    // 创建一个新的局部符号表
    void push() {
        size_t level = symbolStack.size();
        log("sym_table") << "[" << level << "->" << (level + 1) << "] push" << std::endl;

        // 创建一个空的符号表作为局部符号表
        symbolStack.emplace_back();
    }

    // 弹出当前作用域的局部符号表
    void pop() {
        size_t level = symbolStack.size();
        log("sym_table") << "[" << level << "->" << (level - 1) << "] pop" << std::endl;

        //当前作用域结束，弹出局部符号表
        symbolStack.pop_back();
    }

    // 向当前作用域的局部符号表插入一个符号
    void insert(const std::string &name, Ty value) {
        size_t level = symbolStack.size();
        log("sym_table") << "[" << level << "] insert '" << name << "'" << std::endl;

        // 判断重复情况
        auto &currScope = symbolStack.back();
        if (currScope.find(name) != currScope.end()) {
            throw std::runtime_error("symbol '" + name + "' already exists");
        }

        // 插入一个符号到当前作用域的符号表
        symbolStack.back()[name] = value;
    }

    // 从当前作用域开始向上查找符号
    Ty tryLookup(const std::string &name) {
        size_t level = symbolStack.size();

        // 从当前作用域开始向上查找符号
        for (auto it = symbolStack.rbegin(); it != symbolStack.rend(); it++) {
            log("sym_table") << "[" << level-- << "] find '" << name << "'" << std::endl;
            auto item = it->find(name);
            if (item != it->end()) {
                return item->second;
            }
        }

        // 如果找不到，则返回nullptr
        log("sym_table") << "'" << name << "' not found" << std::endl;
        return nullptr;
    }

    // 当查找不到时，抛出异常
    Ty lookup(const std::string &name) {
        auto value = tryLookup(name);
        if (value == nullptr) {
            throw std::runtime_error("symbol '" + name + "' not found");
        }
        return value;
    }
};

#endif //SYSY_COMPILER_FRONTEND_SYMBOL_TABLE_H
