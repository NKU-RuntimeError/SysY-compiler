#include <llvm/IR/Value.h>
#include "AST.h"

llvm::Value *AST::CompileUnit::codeGen() {
    return nullptr;
}

llvm::Value *AST::InitializerElement::codeGen() {
    return nullptr;
}

llvm::Value *AST::InitializerList::codeGen() {
    return nullptr;
}

llvm::Value *AST::ConstVariableDef::codeGen() {
    return nullptr;
}

llvm::Value *AST::ConstVariableDecl::codeGen() {
    return nullptr;
}

llvm::Value *AST::VariableDef::codeGen() {
    return nullptr;
}

llvm::Value *AST::VariableDecl::codeGen() {
    return nullptr;
}

llvm::Value *AST::FunctionArg::codeGen() {
    return nullptr;
}

llvm::Value *AST::Block::codeGen() {
    return nullptr;
}

llvm::Value *AST::FunctionDef::codeGen() {
    return nullptr;
}

llvm::Value *AST::LValue::codeGen() {
    return nullptr;
}

llvm::Value *AST::AssignStmt::codeGen() {
    return nullptr;
}

llvm::Value *AST::NullStmt::codeGen() {
    return nullptr;
}

llvm::Value *AST::ExprStmt::codeGen() {
    return nullptr;
}

llvm::Value *AST::BlockStmt::codeGen() {
    return nullptr;
}

llvm::Value *AST::IfStmt::codeGen() {
    return nullptr;
}

llvm::Value *AST::WhileStmt::codeGen() {
    return nullptr;
}

llvm::Value *AST::BreakStmt::codeGen() {
    return nullptr;
}

llvm::Value *AST::ContinueStmt::codeGen() {
    return nullptr;
}

llvm::Value *AST::ReturnStmt::codeGen() {
    return nullptr;
}

llvm::Value *AST::UnaryExpr::codeGen() {
    return nullptr;
}

llvm::Value *AST::FunctionCallExpr::codeGen() {
    return nullptr;
}

llvm::Value *AST::BinaryExpr::codeGen() {
    return nullptr;
}

llvm::Value *AST::NumberExpr::codeGen() {
    return nullptr;
}

llvm::Value *AST::VariableExpr::codeGen() {
    return nullptr;
}
