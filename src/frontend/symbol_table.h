#ifndef SYSY_COMPILER_FRONTEND_SYMBOL_TABLE_H
#define SYSY_COMPILER_FRONTEND_SYMBOL_TABLE_H

#include "llvm/IR/Value.h"
#include <list>
#include <unordered_map>
#include <string>

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

    std::list<std::unordered_map<std::string, llvm::Value *>> symbolStack;
public:
    // 构造函数，负责创建一个全局符号表
    SymbolTable();

    // 创建一个新的局部符号表
    void push();

    // 弹出当前作用域的局部符号表
    void pop();

    // 向当前作用域的局部符号表插入一个符号
    void insert(const std::string &name, llvm::Value *value);

    // 从当前作用域开始向上查找符号
    llvm::Value *lookup(const std::string &name);
};

#endif //SYSY_COMPILER_FRONTEND_SYMBOL_TABLE_H
