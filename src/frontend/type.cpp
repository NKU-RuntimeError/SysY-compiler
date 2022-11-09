#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include "IR.h"
#include "type.h"

Typename TypeSystem::fromType(llvm::Type *type) {
    if (type->isIntegerTy(32)) {
        return Typename::INT;
    }
    if (type->isIntegerTy(1)) {
        return Typename::BOOL;
    }
    if (type->isFloatTy()) {
        return Typename::FLOAT;
    }

    throw std::runtime_error("value with unknown type");
}

Typename TypeSystem::fromValue(llvm::Value *value) {
    return fromType(value->getType());
}

llvm::Value *TypeSystem::cast(llvm::Value *value, Typename wantType) {

    // 获取当前类型
    Typename currType = fromValue(value);

    // 增加节点，实现类型转换

    // bool -> int
    if (currType == Typename::BOOL && wantType == Typename::INT) {
        return IR::ctx.builder.CreateZExt(
                value,
                llvm::Type::getInt32Ty(IR::ctx.llvmCtx)
        );
    }

    // bool -> float
    if (currType == Typename::BOOL && wantType == Typename::FLOAT) {
        return IR::ctx.builder.CreateUIToFP(
                value,
                llvm::Type::getFloatTy(IR::ctx.llvmCtx)
        );
    }

    // int -> bool
    if (currType == Typename::INT && wantType == Typename::BOOL) {
        return IR::ctx.builder.CreateICmpNE(
                value,
                llvm::ConstantInt::get(
                        llvm::Type::getInt32Ty(IR::ctx.llvmCtx),
                        0
                )
        );
    }

    // int -> float
    if (currType == Typename::INT && wantType == Typename::FLOAT) {
        return IR::ctx.builder.CreateSIToFP(
                value,
                llvm::Type::getFloatTy(IR::ctx.llvmCtx)
        );
    }

    // float -> bool
    if (currType == Typename::FLOAT && wantType == Typename::BOOL) {
        return IR::ctx.builder.CreateFCmpONE(
                value,
                llvm::ConstantFP::get(
                        llvm::Type::getFloatTy(IR::ctx.llvmCtx),
                        0.0
                )
        );
    }

    // float -> int
    if (currType == Typename::FLOAT && wantType == Typename::INT) {
        return IR::ctx.builder.CreateFPToSI(
                value,
                llvm::Type::getInt32Ty(IR::ctx.llvmCtx)
        );
    }

    throw std::runtime_error("unknown type cast");
}

llvm::Type *TypeSystem::get(Typename type) {
    switch (type) {
        case Typename::VOID:
            return llvm::Type::getVoidTy(IR::ctx.llvmCtx);
        case Typename::BOOL:
            return llvm::Type::getInt1Ty(IR::ctx.llvmCtx);
        case Typename::INT:
            return llvm::Type::getInt32Ty(IR::ctx.llvmCtx);
        case Typename::FLOAT:
            return llvm::Type::getFloatTy(IR::ctx.llvmCtx);
    }
    throw std::runtime_error("unknown type");
}

static void arraySizeSanityCheck(const std::vector<std::optional<int>> &size) {
    for (size_t i = 0; i < size.size(); i++) {
        // 除第一维外，后续维度必须有值
        if (i > 0 && !size[i]) {
            throw std::runtime_error("invalid array size structure");
        }

        // 所有维度，只要有值，必须>=0
        // 参考SysY语言定义：
        // "ConstDef中表示各维长度的ConstExp都必须能在编译时求值到非负整数。"
        if (size[i] && *size[i] < 0) {
            throw std::runtime_error("invalid array size value");
        }
    }
}

llvm::Type *TypeSystem::get(Typename type, const std::vector<std::optional<int>> &size) {
    // 数组维度合法性检查
    arraySizeSanityCheck(size);

    // 获取基类型
    llvm::Type *currType = get(type);

    // 从后向前，逐层获取数组类型
    for (auto it = size.rbegin(); it != size.rend(); it++) {
        if (auto currSizeOpt = *it) {
            currType = llvm::ArrayType::get(currType, *currSizeOpt);
        } else {
            currType = llvm::PointerType::get(currType, 0);
        }
    }

    return currType;
}

