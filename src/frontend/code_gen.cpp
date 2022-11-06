#include <stdexcept>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h>
#include "magic_enum.h"
#include "lib.h"
#include "IR.h"
#include "AST.h"

llvm::Value *AST::CompileUnit::codeGen() {
    // 在编译的初始阶段添加SysY系统函数原型
    addLibraryPrototype();

    for (auto compileElement: compileElements) {
        compileElement->codeGen();
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

        // 常量求值阶段可确保数组维度的合法性，因此dynamic_cast不会返回空指针，并且一定是>=0的整型常数
        auto pNumber = dynamic_cast<AST::NumberExpr *>(s);
        result.emplace_back(std::get<int>(pNumber->value));
    }
    return result;
}

static llvm::Constant *
constantInitValConvert(AST::InitializerElement *initializerElement, llvm::Type *type) {
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

static std::vector<llvm::Value *>
getGEPIndices(const std::vector<int> &indices) {
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

static void dynamicInitValCodeGen(
        llvm::Value *alloca,
        AST::InitializerElement *initializerElement,
        const std::vector<int> &indices = {}
) {
    if (std::holds_alternative<AST::Expr *>(initializerElement->element)) {
        auto val = std::get<AST::Expr *>(initializerElement->element)->codeGen();
        auto var = IR::ctx.builder.CreateGEP(
                alloca->getType()->getPointerElementType(),
                alloca,
                getGEPIndices(indices)
        );
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

llvm::Value *AST::ConstVariableDecl::codeGen() {
    // 常量均存储在全局空间
    for (auto def: constVariableDefs) {
        // 确定常量名称，若为局部常量，则补充函数前缀
        std::string varName = def->name;
        if (IR::ctx.local) {
            auto func = IR::ctx.builder.GetInsertBlock()->getParent();
            varName = func->getName().str() + "." + varName;
        }

        auto var = new llvm::GlobalVariable(
                IR::ctx.module,
                TypeSystem::get(type, convertArraySize(def->size)),
                true,
                llvm::GlobalValue::LinkageTypes::InternalLinkage,
                nullptr,
                varName
        );

        // 将常量插入到符号表
        IR::ctx.symbolTable.insert(def->name, var);

        // 初始化
        llvm::Constant *initVal = constantInitValConvert(
                def->initVal,
                TypeSystem::get(type, convertArraySize(def->size))
        );
        var->setInitializer(initVal);
    }

    return nullptr;
}

llvm::Value *AST::VariableDecl::codeGen() {
    // 生成局部变量/全局变量
    if (IR::ctx.local) {
        // 局部变量
        for (auto def: variableDefs) {
            // 生成局部变量
            llvm::AllocaInst *alloca = IR::ctx.builder.CreateAlloca(
                    TypeSystem::get(type, convertArraySize(def->size)),
                    nullptr,
                    def->name
            );

            // 将局部变量插入符号表
            IR::ctx.symbolTable.insert(def->name, alloca);

            // 初始化
            if (def->initVal) {
                dynamicInitValCodeGen(alloca, def->initVal);
            }
        }
    } else {
        // 全局变量
        for (auto def: variableDefs) {
            // 生成全局变量
            auto var = new llvm::GlobalVariable(
                    IR::ctx.module,
                    TypeSystem::get(type, convertArraySize(def->size)),
                    false,
                    llvm::GlobalValue::LinkageTypes::InternalLinkage,
                    nullptr,
                    def->name
            );

            // 将全局变量插入符号表
            IR::ctx.symbolTable.insert(def->name, var);

            // 初始化
            if (def->initVal) {
                llvm::Constant *initVal = constantInitValConvert(
                        def->initVal,
                        TypeSystem::get(type, convertArraySize(def->size))
                );
                var->setInitializer(initVal);
            }
        }
    }

    return nullptr;
}

llvm::Value *AST::Block::codeGen() {
    for (auto element: elements) {
        element->codeGen();
    }
    return nullptr;
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
    // main函数为外部链接，其他函数为内部链接，便于优化
    llvm::Function *function = llvm::Function::Create(
            functionType,
            name == "main" ?
                llvm::Function::ExternalLinkage :
                llvm::Function::InternalLinkage,
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
    IR::ctx.local = true;
    IR::ctx.symbolTable.push();

    // 为参数开空间，并保存在符号表中
    i = 0;
    for (auto &arg: function->args()) {
        llvm::AllocaInst *alloca = IR::ctx.builder.CreateAlloca(
                arg.getType(),
                nullptr,
                arg.getName()
        );
        IR::ctx.builder.CreateStore(&arg, alloca);
        IR::ctx.symbolTable.insert(arguments[i++]->name, alloca);
    }

    // 生成函数体代码
    body->codeGen();

    // 退出作用域
    IR::ctx.symbolTable.pop();
    IR::ctx.local = false;

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
    // 什么也不做
    return nullptr;
}

llvm::Value *AST::ExprStmt::codeGen() {
    // 什么也不做
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
    if (std::holds_alternative<int>(value)) {
        return llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(IR::ctx.llvmCtx),
                std::get<int>(value)
        );
    } else {
        return llvm::ConstantFP::get(
                llvm::Type::getFloatTy(IR::ctx.llvmCtx),
                std::get<float>(value)
        );
    }
}

llvm::Value *AST::VariableExpr::codeGen() {
    llvm::Value *var = IR::ctx.symbolTable.lookup(name);

    // 若为数组，则使用getelementptr指令访问元素
    if (!size.empty()) {
        std::vector<llvm::Value *> indices;

        // 计算各维度
        // 每个getelementptr多一个前缀维度0的原因：
        // https://www.llvm.org/docs/GetElementPtr.html#why-is-the-extra-0-index-required
        indices.emplace_back(llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(IR::ctx.llvmCtx),
                0
        ));
        for (auto s: size) {
            indices.emplace_back(s->codeGen());
        }

        var = IR::ctx.builder.CreateGEP(
                var->getType()->getPointerElementType(),
                var,
                indices
        );
    }

    return IR::ctx.builder.CreateLoad(var->getType()->getPointerElementType(), var);
}
