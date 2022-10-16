#include "log.h"
#include "symbol_table.h"

SymbolTable::SymbolTable() {
    log("sym_table") << "new symbol table" << std::endl;

    // 创建一个空的符号表作为全局符号表
    symbolStack.emplace_back();
}

void SymbolTable::push() {
    size_t level = symbolStack.size();
    log("sym_table") << "[" << level << "->" << (level + 1) << "] push" << std::endl;

    // 创建一个空的符号表作为局部符号表
    symbolStack.emplace_back();
}

void SymbolTable::pop() {
    size_t level = symbolStack.size();
    log("sym_table") << "[" << level << "->" << (level - 1) << "] pop" << std::endl;

    //当前作用域结束，弹出局部符号表
    symbolStack.pop_back();
}

void SymbolTable::insert(const std::string &name, llvm::Value *value) {
    size_t level = symbolStack.size();
    log("sym_table") << "[" << level << "] insert '" << name << "'" << std::endl;

    // 插入一个符号到当前作用域的符号表
    symbolStack.back()[name] = value;
}

llvm::Value *SymbolTable::lookup(const std::string &name) {
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
    err("sym_table") << "'" << name << "' not found" << std::endl;
    return nullptr;
}
