#ifndef SYSY_COMPILER_FRONTEND_CODE_GEN_HELPER_H
#define SYSY_COMPILER_FRONTEND_CODE_GEN_HELPER_H

#include <tuple>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include "AST.h"
#include "type.h"

namespace CodeGenHelper {

    // 一元运算符类型修正（类型范围）
    std::tuple<llvm::Value *, Typename>
    unaryExprTypeFix(
            llvm::Value *value,
            Typename minType,
            Typename maxType
    );

    // 一元运算符类型修正（指定类型）
    llvm::Value *
    unaryExprTypeFix(
            llvm::Value *value,
            Typename wantType
    );

    // 二元运算符类型修正（类型范围）
    std::tuple<llvm::Value *, llvm::Value *, Typename>
    binaryExprTypeFix(
            llvm::Value *L,
            llvm::Value *R,
            Typename minType,
            Typename maxType
    );

    // 二元运算符类型修正（指定类型）
    std::tuple<llvm::Value *, llvm::Value *>
    binaryExprTypeFix(
            llvm::Value *L,
            llvm::Value *R,
            Typename wantType
    );

    // 数组维度信息转换（Expr* -> int）
    std::vector<std::optional<int>>
    convertArraySize(
            std::vector<AST::Expr *> &size
    );

    // 数组常量初值转换，用于全局常量数组，全局变量数组，局部常量数组（生成LLVM Constant）
    llvm::Constant *
    constantInitValConvert(
            AST::InitializerElement *initializerElement,
            llvm::Type *type
    );

    // 生成数组索引（添加GEP的前缀0）
    std::vector<llvm::Value *>
    getGEPIndices(
            const std::vector<int> &indices
    );

    // 局部变量数组初值赋值代码生成
    void
    dynamicInitValCodeGen(
            llvm::Value *alloca,
            AST::InitializerElement *initializerElement,
            const std::vector<int> &indices = {}
    );

    // 获取变量指针，支持数组做参数，局部变量数组，等所有需要获得元素指针的情况
    // 根据每层的不同类型，使用到GEP和load指令，确保其通用性
    llvm::Value *
    getVariablePointer(
            const std::string &name,
            const std::vector<AST::Expr *> &size
    );

}

#endif //SYSY_COMPILER_FRONTEND_CODE_GEN_HELPER_H
