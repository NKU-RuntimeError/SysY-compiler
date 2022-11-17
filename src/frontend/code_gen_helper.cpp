#include <tuple>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include "AST.h"
#include "IR.h"
#include "type.h"
#include "code_gen_helper.h"

using namespace CodeGenHelper;

std::tuple<llvm::Value *, Typename>
CodeGenHelper::unaryExprTypeFix(
        llvm::Value *value,
        Typename minType,
        Typename maxType
) {
    Typename type = TypeSystem::from(value);

    // 获得该节点的计算类型
    Typename calcType = static_cast<Typename>(std::clamp(
            static_cast<int>(type),
            static_cast<int>(minType),
            static_cast<int>(maxType)
    ));

    // 按需进行类型转换
    if (type != calcType) {
        value = TypeSystem::cast(value, calcType);
    }

    // 返回转换结果、计算类型
    return {value, calcType};
}

llvm::Value *
CodeGenHelper::unaryExprTypeFix(
        llvm::Value *value,
        Typename wantType
) {
    return std::get<0>(unaryExprTypeFix(value, wantType, wantType));
}

std::tuple<llvm::Value *, llvm::Value *, Typename>
CodeGenHelper::binaryExprTypeFix(
        llvm::Value *L,
        llvm::Value *R,
        Typename minType,
        Typename maxType
) {
    // 获得左右子树类型
    Typename LType = TypeSystem::from(L);
    Typename RType = TypeSystem::from(R);

    // 获得该节点的计算类型
    Typename calcType;

    // 取max是因为在Typename中编号按照类型优先级排列，越大优先级越高
    // 对于每个二元运算，我们希望类型向高处转换，保证计算精度
    calcType = static_cast<Typename>(std::max(
            static_cast<int>(LType),
            static_cast<int>(RType)
    ));

    calcType = static_cast<Typename>(std::clamp(
            static_cast<int>(calcType),
            static_cast<int>(minType),
            static_cast<int>(maxType)
    ));

    // 按需进行类型转换
    if (LType != calcType) {
        L = TypeSystem::cast(L, calcType);
    }
    if (RType != calcType) {
        R = TypeSystem::cast(R, calcType);
    }

    // 返回转换结果、计算类型
    return {L, R, calcType};
}

std::tuple<llvm::Value *, llvm::Value *>
CodeGenHelper::binaryExprTypeFix(
        llvm::Value *L,
        llvm::Value *R,
        Typename wantType
) {
    auto [LFixed, RFixed, calcType] =
            binaryExprTypeFix(L, R, wantType, wantType);
    return {LFixed, RFixed};
}

std::vector<std::optional<int>>
CodeGenHelper::convertArraySize(
        std::vector<AST::Expr *> &size
) {
    std::vector<std::optional<int>> result;
    for (auto s: size) {
        // 处理空指针
        if (!s) {
            result.emplace_back(std::nullopt);
            continue;
        }

        // 常量求值阶段可确保数组维度的合法性，因此dynamic_cast不会返回空指针，并且一定是>=0的整型常数
        auto pNumber = dynamic_cast<AST::NumberExpr *>(s);
        result.emplace_back(std::get<int>(pNumber->value));
    }
    return result;
}

llvm::Constant *
CodeGenHelper::constantInitValConvert(
        AST::InitializerElement *initializerElement,
        llvm::Type *type
) {
    // 递归出口
    if (std::holds_alternative<AST::Expr *>(initializerElement->element)) {
        return llvm::cast<llvm::Constant>(
                std::get<AST::Expr *>(initializerElement->element)->codeGen()
        );
    }

    auto initializerList = std::get<AST::InitializerList *>(
            initializerElement->element
    );

    std::vector<llvm::Constant *> initVals;

    // 递归将InitializerList转换为llvm::Constant
    for (auto element: initializerList->elements) {
        auto initVal = constantInitValConvert(
                element,
                type->getArrayElementType()
        );
        initVals.emplace_back(initVal);
    }

    return llvm::ConstantArray::get(
            llvm::cast<llvm::ArrayType>(type),
            initVals
    );
}

std::vector<llvm::Value *>
CodeGenHelper::getGEPIndices(
        const std::vector<int> &indices
) {
    std::vector<llvm::Value *> GEPIndices;
    GEPIndices.emplace_back(
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(IR::ctx.llvmCtx), 0)
    );
    for (int index: indices) {
        GEPIndices.emplace_back(
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(IR::ctx.llvmCtx), index)
        );
    }
    return GEPIndices;
}

void
CodeGenHelper::dynamicInitValCodeGen(
        llvm::Value *alloca,
        AST::InitializerElement *initializerElement,
        const std::vector<int> &indices
) {
    if (std::holds_alternative<AST::Expr *>(initializerElement->element)) {
        auto val = std::get<AST::Expr *>(initializerElement->element)->codeGen();
        auto var = IR::ctx.builder.CreateGEP(
                alloca->getType()->getPointerElementType(),
                alloca,
                getGEPIndices(indices)
        );
        // 普通数组初值隐式类型转换
        Typename wantType = TypeSystem::from(var->getType()->getPointerElementType());
        val = unaryExprTypeFix(val, wantType);
        IR::ctx.builder.CreateStore(val, var);
        return;
    }

    auto initializerList = std::get<AST::InitializerList *>(
            initializerElement->element
    );

    std::vector<int> nextIndices = indices;
    for (int i = 0; i < initializerList->elements.size(); i++) {
        nextIndices.emplace_back(i);

        dynamicInitValCodeGen(
                alloca,
                initializerList->elements[i],
                nextIndices
        );

        nextIndices.pop_back();
    }
}

llvm::Value *
CodeGenHelper::getVariablePointer(
        const std::string &name,
        const std::vector<AST::Expr *> &size
) {
    llvm::Value *var = IR::ctx.symbolTable.lookup(name);

    // 计算维度
    std::vector<llvm::Value *> indices;
    for (auto s: size) {
        indices.emplace_back(s->codeGen());
    }

    // 寻址
    for (auto index: indices) {
        if (var->getType()->getPointerElementType()->isPointerTy()) {
            var = IR::ctx.builder.CreateLoad(
                    var->getType()->getPointerElementType(),
                    var
            );
            var = IR::ctx.builder.CreateGEP(
                    var->getType()->getPointerElementType(),
                    var,
                    index
            );
        } else {
            var = IR::ctx.builder.CreateGEP(
                    var->getType()->getPointerElementType(),
                    var,
                    {
                            llvm::ConstantInt::get(
                                    llvm::Type::getInt32Ty(IR::ctx.llvmCtx),
                                    0
                            ),
                            index
                    }
            );
        }
    }
    return var;
}
