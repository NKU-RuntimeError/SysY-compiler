#include <stdexcept>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h>
#include "magic_enum.h"
#include "IR.h"
#include "AST.h"

llvm::Value *AST::CompileUnit::codeGen() {
    for (auto compileElement: compileElements) {
        compileElement->codeGen();
    }
    return nullptr;
}

llvm::Value *AST::ConstVariableDecl::codeGen() {
    return nullptr;
}

llvm::Value *AST::VariableDecl::codeGen() {
    return nullptr;
}

llvm::Value *AST::Block::codeGen() {
    for (auto element: elements) {
        element->codeGen();
    }
    return nullptr;
}

static std::vector<std::optional<int>>
convertArraySize(std::vector<AST::Expr *> &size) {
    std::vector<std::optional<int>> result;
    for (auto s: size) {
        // 处理空指针
        if (!s) {
            result.emplace_back(std::nullopt);
            continue;
        }

        // 若此行代码抛出异常，说明数组大小不是常量
        auto pNumber = dynamic_cast<AST::NumberExpr *>(s);

        // 若此行代码抛出异常，说明数组大小不是整数，编译期常量求值错误
        result.emplace_back(std::get<int>(pNumber->value));
    }
    return result;
}

llvm::Value *AST::FunctionDef::codeGen() {
    // 计算参数类型
    std::vector<llvm::Type *> argTypes;
    for (auto argument: arguments) {
        // 该函数对普通类型和数组均适用，因此不对类型进行区分
        argTypes.emplace_back(TypeSystem::get(
                argument->type,
                convertArraySize(argument->size)
        ));
    }

    // 创建函数类型
    llvm::FunctionType *functionType = llvm::FunctionType::get(
            TypeSystem::get(returnType),
            argTypes,
            false
    );

    // 创建函数
    llvm::Function *function = llvm::Function::Create(
            functionType,
            llvm::Function::ExternalLinkage,
            name,
            IR::ctx.module
    );

    // 设置参数名
    size_t i = 0;
    for (auto &arg: function->args()) {
        arg.setName(arguments[i++]->name);
    }

    // 创建入口基本块
    llvm::BasicBlock *entryBlock = llvm::BasicBlock::Create(
            IR::ctx.llvmCtx,
            "entry",
            function
    );

    // 设置当前插入点
    IR::ctx.builder.SetInsertPoint(entryBlock);

    // 进入新的作用域
    IR::ctx.symbolTable.push();

    // 为参数开空间，并保存在符号表中
    i = 0;
    for (auto &arg: function->args()) {
        llvm::AllocaInst *alloca = IR::ctx.builder.CreateAlloca(
                arg.getType(),
                nullptr,
                "p" + arg.getName()
        );
        IR::ctx.builder.CreateStore(&arg, alloca);
        IR::ctx.symbolTable.insert(arguments[i++]->name, alloca);
    }

    // 生成函数体代码
    body->codeGen();

    // 退出作用域
    IR::ctx.symbolTable.pop();

    // 验证函数
    if (llvm::verifyFunction(*function)) {
        throw std::logic_error("function verification failed");
    }

    return function;
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
    if (expr) {
        IR::ctx.builder.CreateRet(expr->codeGen());
    } else {
        IR::ctx.builder.CreateRetVoid();
    }
    return nullptr;
}

llvm::Value *AST::UnaryExpr::codeGen() {
    return nullptr;
}

llvm::Value *AST::FunctionCallExpr::codeGen() {
    return nullptr;
}

static std::tuple<llvm::Value *, llvm::Value *, Typename>
binaryExprTypeFix(llvm::Value *L, llvm::Value *R) {
    // 获得左右子树类型
    Typename LType = TypeSystem::fromValue(L);
    Typename RType = TypeSystem::fromValue(R);

    // 获得该节点的计算类型
    // 取max是因为在Typename中编号按照类型优先级排列，越大优先级越高
    // 对于每个二元运算，我们希望类型向高处转换，保证计算精度
    Typename nodeType = static_cast<Typename>(std::max(
            static_cast<int>(LType),
            static_cast<int>(RType)
    ));

    // 按需进行类型转换
    if (LType != nodeType) {
        L = TypeSystem::cast(L, nodeType);
    }
    if (RType != nodeType) {
        R = TypeSystem::cast(R, nodeType);
    }

    // 返回转换结果、计算类型
    return {L, R, nodeType};
}

llvm::Value *AST::BinaryExpr::codeGen() {
    // 生成子表达式
    llvm::Value *L = lhs->codeGen();
    llvm::Value *R = rhs->codeGen();

    // 对子表达式进行隐式类型转换
    auto [LFix, RFix, nodeType] = binaryExprTypeFix(L, R);

    // 组合获得新表达式
    switch (op) {

        // 算数运算
        case Operator::ADD: {
            if (nodeType == Typename::INT) {
                return IR::ctx.builder.CreateAdd(LFix, RFix);
            }
            if (nodeType == Typename::FLOAT) {
                return IR::ctx.builder.CreateFAdd(LFix, RFix);
            }
            throw std::runtime_error("invalid type for operator +");
        }
//        case Operator::SUB:
//            return nullptr;
//        case Operator::MUL:
//            return nullptr;
//        case Operator::DIV:
//            return nullptr;
//        case Operator::MOD:
//            return nullptr;
//
//        // 逻辑运算
//        case Operator::AND:
//            return nullptr;
//        case Operator::OR:
//            return nullptr;
//
//        // 关系运算
//        case Operator::LT:
//            return nullptr;
//        case Operator::LE:
//            return nullptr;
//        case Operator::GT:
//            return nullptr;
//        case Operator::GE:
//            return nullptr;
//        case Operator::EQ:
//            return nullptr;
//        case Operator::NE:
//            return nullptr;
    }

    throw std::runtime_error(
            "invalid operator: " + std::string(magic_enum::enum_name(op))
    );
}

llvm::Value *AST::NumberExpr::codeGen() {
    switch (type) {
        case Typename::INT:
            return llvm::ConstantInt::get(
                    llvm::Type::getInt32Ty(IR::ctx.llvmCtx),
                    std::get<int>(value)
            );
        case Typename::FLOAT:
            return llvm::ConstantFP::get(
                    llvm::Type::getFloatTy(IR::ctx.llvmCtx),
                    std::get<float>(value)
            );
    }

    throw std::runtime_error(
            "invalid type: " + std::string(magic_enum::enum_name(type))
    );
}

llvm::Value *AST::VariableExpr::codeGen() {
    return nullptr;
}
