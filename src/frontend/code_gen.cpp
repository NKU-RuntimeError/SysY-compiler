#include <stdexcept>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h>
#include "magic_enum.h"
#include "lib.h"
#include "IR.h"
#include "AST.h"

////////////////////////////////////////////////////////////////////////////////
// helper函数

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
        // 普通数组初值隐式类型转换
        Typename wantType = TypeSystem::fromType(var->getType()->getPointerElementType());
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

static llvm::Value *
getVariablePointer(const std::string &name, const std::vector<AST::Expr *> &size) {
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

    return var;
}

static std::tuple<llvm::Value *, Typename>
unaryExprTypeFix(llvm::Value *value, Typename minType, Typename maxType) {
    Typename type = TypeSystem::fromValue(value);

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

inline static llvm::Value *
unaryExprTypeFix(llvm::Value *value, Typename wantType) {
    return std::get<0>(unaryExprTypeFix(value, wantType, wantType));
}

static std::tuple<llvm::Value *, llvm::Value *, Typename>
binaryExprTypeFix(llvm::Value *L, llvm::Value *R, Typename minType, Typename maxType) {
    // 获得左右子树类型
    Typename LType = TypeSystem::fromValue(L);
    Typename RType = TypeSystem::fromValue(R);

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

inline static std::tuple<llvm::Value *, llvm::Value *>
binaryExprTypeFix(llvm::Value *L, llvm::Value *R, Typename wantType) {
    auto [LFixed, RFixed, calcType] =
            binaryExprTypeFix(L, R, wantType, wantType);
    return {LFixed, RFixed};
}

////////////////////////////////////////////////////////////////////////////////
// IR生成函数

llvm::Value *AST::CompileUnit::codeGen() {
    // 在编译的初始阶段添加SysY系统函数原型
    addLibraryPrototype();

    for (auto compileElement: compileElements) {
        compileElement->codeGen();
    }
    return nullptr;
}

llvm::Value *AST::ConstVariableDecl::codeGen() {
    // 常量均存储在全局空间
    for (auto def: constVariableDefs) {
        // 确定常量名称，若为局部常量，则补充函数前缀
        std::string varName;
        if (IR::ctx.local) {
            auto func = IR::ctx.builder.GetInsertBlock()->getParent();
            varName = func->getName().str() + "." + def->name;
        } else {
            varName = def->name;
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
            } else {
                // 未初始化的全局变量默认初始化为0
                var->setInitializer(llvm::Constant::getNullValue(
                        TypeSystem::get(type, convertArraySize(def->size))
                ));
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

    // void函数需要在没有terminator的BB后面插入一个ret void
    if (returnType == Typename::VOID) {
        for (auto &BB : function->getBasicBlockList()) {
            if (BB.getTerminator() == nullptr) {
                IR::ctx.builder.SetInsertPoint(&BB);
                IR::ctx.builder.CreateRetVoid();
            }
        }
    }

    // 验证函数
    if (llvm::verifyFunction(*function, &llvm::outs())) {
        IR::show();
        throw std::logic_error("function verification failed");
    }

    return nullptr;
}

llvm::Value *AST::AssignStmt::codeGen() {
    // 获取左值和右值
    llvm::Value *lhs = getVariablePointer(lValue->name, lValue->size);
    llvm::Value *rhs = rValue->codeGen();

    // 获取变量类型
    // 注意：左值是变量的指针，需要获取其指向的类型
    Typename lType = TypeSystem::fromType(lhs->getType()->getPointerElementType());
    Typename rType = TypeSystem::fromValue(rhs);

    // 尝试进行隐式类型转换，失败则抛出异常
    if (lType != rType) {
        rhs = TypeSystem::cast(rhs, lType);
    }

    IR::ctx.builder.CreateStore(rhs, lhs);

    // SysY中的赋值语句没有值，因此返回空指针即可
    return nullptr;
}

llvm::Value *AST::NullStmt::codeGen() {
    // 什么也不做
    return nullptr;
}

llvm::Value *AST::ExprStmt::codeGen() {
    expr->codeGen();
    return nullptr;
}

llvm::Value *AST::BlockStmt::codeGen() {
    // 块语句需要开启新一层作用域
    IR::ctx.symbolTable.push();
    for (auto element: elements) {
        element->codeGen();
    }
    IR::ctx.symbolTable.pop();
    return nullptr;
}

llvm::Value *AST::IfStmt::codeGen() {
    // 计算条件表达式
    llvm::Value *value = condition->codeGen();

    // 隐式类型转换
    value = unaryExprTypeFix(value, Typename::BOOL);

    llvm::Function *function = IR::ctx.builder.GetInsertBlock()->getParent();
    llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(IR::ctx.llvmCtx, "then");
    llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(IR::ctx.llvmCtx, "else");
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(IR::ctx.llvmCtx, "merge");

    IR::ctx.builder.CreateCondBr(value, thenBB, elseBB);

    // merge块不一定是需要的
    // 仅当if或else分支需要跳转到merge块的时候，才会将merge块放到函数中
    bool needMergeBB = false;

    // 真分支
    function->getBasicBlockList().push_back(thenBB);
    IR::ctx.builder.SetInsertPoint(thenBB);
    thenStmt->codeGen();
    // 如果插入点还存在，说明没有产生return
    // 此时创建无条件指令跳转到merge块
    if (IR::ctx.builder.GetInsertBlock()) {
        needMergeBB = true;
        IR::ctx.builder.CreateBr(mergeBB);
    }

    // 假分支
    // if, if-else 均有假分支，if的假分支直接跳转到merge基本块
    // 让pass来帮我们优化吧
    function->getBasicBlockList().push_back(elseBB);
    IR::ctx.builder.SetInsertPoint(elseBB);
    if (elseStmt) {
        elseStmt->codeGen();
    }
    // 如果插入点还存在，说明没有产生return
    // 此时创建无条件指令跳转到merge块
    if (IR::ctx.builder.GetInsertBlock()) {
        needMergeBB = true;
        IR::ctx.builder.CreateBr(mergeBB);
    }

    // 合并块
    // if (xxx) return X; else return Y;
    // 在这种情况下，merge块是不需要的
    if (needMergeBB) {
        function->getBasicBlockList().push_back(mergeBB);
        IR::ctx.builder.SetInsertPoint(mergeBB);
    }

    return nullptr;
}

llvm::Value *AST::WhileStmt::codeGen() {

    //
    //           |
    // +--->-----+
    // |         V
    // cond:                 <----+
    // |   +------------+         |
    // |   |            |         |
    // |   +------------+         |
    // |         |                |
    // |         +-----------+    | continue target
    // |         V           |    |
    // body:                 |    |
    // |   +------------+    |    |
    // |   |            +---------+
    // |   +------------+    |    |
    // |         |           |    |
    // +---------+           |    |
    //                       |    | break target
    //           +-----------+    |
    //           V                |
    // cont:                 <----+
    //

    if (!IR::ctx.builder.GetInsertBlock()) {
        return nullptr;
    }

    llvm::Function *function = IR::ctx.builder.GetInsertBlock()->getParent();
    llvm::BasicBlock *conditionBB = llvm::BasicBlock::Create(IR::ctx.llvmCtx, "cond");
    llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(IR::ctx.llvmCtx, "body");
    llvm::BasicBlock *continueBB = llvm::BasicBlock::Create(IR::ctx.llvmCtx, "cont");

    IR::ctx.builder.CreateBr(conditionBB);

    // condition基本块
    function->getBasicBlockList().push_back(conditionBB);
    IR::ctx.builder.SetInsertPoint(conditionBB);

    // 计算条件表达式
    llvm::Value *value = condition->codeGen();

    // 隐式类型转换
    Typename type = TypeSystem::fromValue(value);
    if (type != Typename::BOOL) {
        value = TypeSystem::cast(value, Typename::BOOL);
    }

    // 跳转到body基本块
    IR::ctx.builder.CreateCondBr(value, bodyBB, continueBB);

    // body基本块
    function->getBasicBlockList().push_back(bodyBB);
    IR::ctx.builder.SetInsertPoint(bodyBB);

    // 生成body语句
    IR::ctx.loops.push({conditionBB, continueBB});
    body->codeGen();
    IR::ctx.loops.pop();

    if (IR::ctx.builder.GetInsertBlock()) {
        // 跳转到condition基本块
        IR::ctx.builder.CreateBr(conditionBB);
    }

    // continue基本块
    function->getBasicBlockList().push_back(continueBB);
    IR::ctx.builder.SetInsertPoint(continueBB);

    return nullptr;
}

llvm::Value *AST::BreakStmt::codeGen() {
    if (IR::ctx.loops.empty()) {
        throw std::runtime_error("break statement outside of loop");
    }

    if (!IR::ctx.builder.GetInsertBlock()) {
        return nullptr;
    }

    IR::ctx.builder.CreateBr(IR::ctx.loops.top().breakBB);

    IR::ctx.builder.ClearInsertionPoint();
    return nullptr;
}

llvm::Value *AST::ContinueStmt::codeGen() {
    if (IR::ctx.loops.empty()) {
        throw std::runtime_error("break statement outside of loop");
    }

    if (!IR::ctx.builder.GetInsertBlock()) {
        return nullptr;
    }

    IR::ctx.builder.CreateBr(IR::ctx.loops.top().continueBB);

    IR::ctx.builder.ClearInsertionPoint();
    return nullptr;
}

llvm::Value *AST::ReturnStmt::codeGen() {
    // 如果插入点已经被clear，说明该基本块已经产生了return指令
    // 则不再插入return指令，直接返回
    if (!IR::ctx.builder.GetInsertBlock()) {
        return nullptr;
    }

    if (expr) {
        IR::ctx.builder.CreateRet(expr->codeGen());
    } else {
        IR::ctx.builder.CreateRetVoid();
    }

    // 丢弃后续的IR
    IR::ctx.builder.ClearInsertionPoint();

    return nullptr;
}

llvm::Value *AST::UnaryExpr::codeGen() {
    llvm::Value *value = expr->codeGen();
    switch (op) {
        case Operator::ADD: {
            auto [valueFix, newType] =
                    unaryExprTypeFix(value, Typename::INT, Typename::FLOAT);
            return valueFix;
        }
        case Operator::SUB: {
            auto [valueFix, newType] =
                    unaryExprTypeFix(value, Typename::INT, Typename::FLOAT);
            if (newType == Typename::INT) {
                return IR::ctx.builder.CreateNeg(valueFix);
            }
            if (newType == Typename::FLOAT) {
                return IR::ctx.builder.CreateFNeg(valueFix);
            }
        }
        case Operator::NOT: {
            auto valueFix = unaryExprTypeFix(value, Typename::BOOL);
            return IR::ctx.builder.CreateNot(valueFix);
        }
    }
    throw std::logic_error(
            "invalid operator " + std::string(magic_enum::enum_name(op)) +
            " in unary expression"
    );
}

llvm::Value *AST::FunctionCallExpr::codeGen() {
    // 由于函数不涉及到分层问题，因此并没有存储在自建符号表中
    // 直接使用llvm module中的函数表即可
    llvm::Function *function = IR::ctx.module.getFunction(name);

    // 合法性检查
    if (!function) {
        throw std::runtime_error("function " + name + " not found");
    }
    if (function->arg_size() != params.size()) {
        throw std::runtime_error("invalid number of params for function " + name);
    }

    // 计算实参值
    std::vector<llvm::Value *> values;
    for (auto param: params) {
        values.emplace_back(param->codeGen());
    }

    // 隐式类型转换
    size_t i = 0;
    for (const auto &argument : function->args()) {
        Typename wantType = TypeSystem::fromType(argument.getType());
        Typename gotType = TypeSystem::fromValue(values[i]);
        if (wantType != gotType) {
            values[i] = TypeSystem::cast(values[i], wantType);
        }
        i++;
    }

    // 调用函数
    return IR::ctx.builder.CreateCall(function, values);
}


llvm::Value *AST::BinaryExpr::codeGen() {

    // 组合获得新表达式
    switch (op) {

        // 算数运算
        case Operator::ADD: {
            llvm::Value *L = lhs->codeGen();
            llvm::Value *R = rhs->codeGen();
            auto [LFix, RFix, nodeType] =
                    binaryExprTypeFix(L, R, Typename::INT, Typename::FLOAT);
            if (nodeType == Typename::INT) {
                return IR::ctx.builder.CreateAdd(LFix, RFix);
            }
            if (nodeType == Typename::FLOAT) {
                return IR::ctx.builder.CreateFAdd(LFix, RFix);
            }
        }
        case Operator::SUB: {
            llvm::Value *L = lhs->codeGen();
            llvm::Value *R = rhs->codeGen();
            auto [LFix, RFix, nodeType] =
                    binaryExprTypeFix(L, R, Typename::INT, Typename::FLOAT);
            if (nodeType == Typename::INT) {
                return IR::ctx.builder.CreateSub(LFix, RFix);
            }
            if (nodeType == Typename::FLOAT) {
                return IR::ctx.builder.CreateFSub(LFix, RFix);
            }
        }
        case Operator::MUL: {
            llvm::Value *L = lhs->codeGen();
            llvm::Value *R = rhs->codeGen();
            auto [LFix, RFix, nodeType] =
                    binaryExprTypeFix(L, R, Typename::INT, Typename::FLOAT);
            if (nodeType == Typename::INT) {
                return IR::ctx.builder.CreateMul(LFix, RFix);
            }
            if (nodeType == Typename::FLOAT) {
                return IR::ctx.builder.CreateFMul(LFix, RFix);
            }
        }
        case Operator::DIV: {
            llvm::Value *L = lhs->codeGen();
            llvm::Value *R = rhs->codeGen();
            auto [LFix, RFix, nodeType] =
                    binaryExprTypeFix(L, R, Typename::INT, Typename::FLOAT);
            if (nodeType == Typename::INT) {
                return IR::ctx.builder.CreateSDiv(LFix, RFix);
            }
            if (nodeType == Typename::FLOAT) {
                return IR::ctx.builder.CreateFDiv(LFix, RFix);
            }
        }
        case Operator::MOD: {
            llvm::Value *L = lhs->codeGen();
            llvm::Value *R = rhs->codeGen();
            auto [LFix, RFix, nodeType] =
                    binaryExprTypeFix(L, R, Typename::INT, Typename::FLOAT);
            if (nodeType == Typename::INT) {
                return IR::ctx.builder.CreateSRem(LFix, RFix);
            }
            throw std::runtime_error("invalid type for operator %");
        }

        // 逻辑运算
        case Operator::AND: {

            //
            // +------------+
            // |            |
            // +------------+
            //       |
            //       +----->-----+
            //     T V     F     |
            // and:              |
            // +------------+    |
            // |            |    |
            // +------------+    |
            //       |-----------+
            //       V
            // andm:
            // <PHI>
            //

            if (!IR::ctx.builder.GetInsertBlock()) {
                return nullptr;
            }

            llvm::Function *function = IR::ctx.builder.GetInsertBlock()->getParent();
            llvm::BasicBlock *andBB = llvm::BasicBlock::Create(IR::ctx.llvmCtx, "and");
            llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(IR::ctx.llvmCtx, "andm");

            // 左侧表达式一定会生成
            llvm::Value *L = lhs->codeGen();
            L = unaryExprTypeFix(L, Typename::BOOL);
            IR::ctx.builder.CreateCondBr(L, andBB, mergeBB);
            auto incoming1 = IR::ctx.builder.GetInsertBlock();

            // 生成右侧表达式
            function->getBasicBlockList().push_back(andBB);
            IR::ctx.builder.SetInsertPoint(andBB);

            llvm::Value *R = rhs->codeGen();
            R = unaryExprTypeFix(R, Typename::BOOL);
            IR::ctx.builder.CreateBr(mergeBB);
            auto incoming2 = IR::ctx.builder.GetInsertBlock();

            // 生成合并块
            function->getBasicBlockList().push_back(mergeBB);
            IR::ctx.builder.SetInsertPoint(mergeBB);
            llvm::PHINode *phi = IR::ctx.builder.CreatePHI(
                    llvm::Type::getInt1Ty(IR::ctx.llvmCtx), 2
            );

            // 生成合并块的phi节点，将左右两侧的值传入
            phi->addIncoming(L, incoming1);
            phi->addIncoming(R, incoming2);

            return phi;
        }
        case Operator::OR: {

            //
            // +------------+
            // |            |
            // +------------+
            //       |
            //       +----->-----+
            //     F V     T     |
            // or:               |
            // +------------+    |
            // |            |    |
            // +------------+    |
            //       |-----------+
            //       V
            // orm:
            // <PHI>
            //

            if (!IR::ctx.builder.GetInsertBlock()) {
                return nullptr;
            }

            llvm::Function *function = IR::ctx.builder.GetInsertBlock()->getParent();
            llvm::BasicBlock *orBB = llvm::BasicBlock::Create(IR::ctx.llvmCtx, "or");
            llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(IR::ctx.llvmCtx, "orm");

            // 左侧表达式一定会生成
            llvm::Value *L = lhs->codeGen();
            L = unaryExprTypeFix(L, Typename::BOOL);
            IR::ctx.builder.CreateCondBr(L, mergeBB, orBB);
            auto incoming1 = IR::ctx.builder.GetInsertBlock();

            // 生成右侧表达式
            function->getBasicBlockList().push_back(orBB);
            IR::ctx.builder.SetInsertPoint(orBB);

            llvm::Value *R = rhs->codeGen();
            R = unaryExprTypeFix(R, Typename::BOOL);
            IR::ctx.builder.CreateBr(mergeBB);
            auto incoming2 = IR::ctx.builder.GetInsertBlock();

            // 生成合并块
            function->getBasicBlockList().push_back(mergeBB);
            IR::ctx.builder.SetInsertPoint(mergeBB);
            llvm::PHINode *phi = IR::ctx.builder.CreatePHI(
                    llvm::Type::getInt1Ty(IR::ctx.llvmCtx), 2
            );

            // 生成合并块的phi节点，将左右两侧的值传入
            phi->addIncoming(L, incoming1);
            phi->addIncoming(R, incoming2);

            return phi;
        }

        // 关系运算
        case Operator::LT: {
            llvm::Value *L = lhs->codeGen();
            llvm::Value *R = rhs->codeGen();
            auto [LFix, RFix, nodeType] =
                    binaryExprTypeFix(L, R, Typename::INT, Typename::FLOAT);
            if (nodeType == Typename::INT) {
                return IR::ctx.builder.CreateICmpSLT(LFix, RFix);
            }
            if (nodeType == Typename::FLOAT) {
                return IR::ctx.builder.CreateFCmpOLT(LFix, RFix);
            }
        }
        case Operator::LE: {
            llvm::Value *L = lhs->codeGen();
            llvm::Value *R = rhs->codeGen();
            auto [LFix, RFix, nodeType] =
                    binaryExprTypeFix(L, R, Typename::INT, Typename::FLOAT);
            if (nodeType == Typename::INT) {
                return IR::ctx.builder.CreateICmpSLE(LFix, RFix);
            }
            if (nodeType == Typename::FLOAT) {
                return IR::ctx.builder.CreateFCmpOLE(LFix, RFix);
            }
        }
        case Operator::GT: {
            llvm::Value *L = lhs->codeGen();
            llvm::Value *R = rhs->codeGen();
            auto [LFix, RFix, nodeType] =
                    binaryExprTypeFix(L, R, Typename::INT, Typename::FLOAT);
            if (nodeType == Typename::INT) {
                return IR::ctx.builder.CreateICmpSGT(LFix, RFix);
            }
            if (nodeType == Typename::FLOAT) {
                return IR::ctx.builder.CreateFCmpOGT(LFix, RFix);
            }
        }
        case Operator::GE: {
            llvm::Value *L = lhs->codeGen();
            llvm::Value *R = rhs->codeGen();
            auto [LFix, RFix, nodeType] =
                    binaryExprTypeFix(L, R, Typename::INT, Typename::FLOAT);
            if (nodeType == Typename::INT) {
                return IR::ctx.builder.CreateICmpSGE(LFix, RFix);
            }
            if (nodeType == Typename::FLOAT) {
                return IR::ctx.builder.CreateFCmpOGE(LFix, RFix);
            }
        }
        case Operator::EQ: {
            llvm::Value *L = lhs->codeGen();
            llvm::Value *R = rhs->codeGen();
            auto [LFix, RFix, nodeType] =
                    binaryExprTypeFix(L, R, Typename::INT, Typename::FLOAT);
            if (nodeType == Typename::INT) {
                return IR::ctx.builder.CreateICmpEQ(LFix, RFix);
            }
            if (nodeType == Typename::FLOAT) {
                return IR::ctx.builder.CreateFCmpOEQ(LFix, RFix);
            }
        }
        case Operator::NE: {
            llvm::Value *L = lhs->codeGen();
            llvm::Value *R = rhs->codeGen();
            auto [LFix, RFix, nodeType] =
                    binaryExprTypeFix(L, R, Typename::INT, Typename::FLOAT);
            if (nodeType == Typename::INT) {
                return IR::ctx.builder.CreateICmpNE(LFix, RFix);
            }
            if (nodeType == Typename::FLOAT) {
                return IR::ctx.builder.CreateFCmpONE(LFix, RFix);
            }
        }
    }

    throw std::logic_error(
            "invalid operator: " + std::string(magic_enum::enum_name(op)) +
            " in binary expression"
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
    llvm::Value *var = getVariablePointer(name, size);
    return IR::ctx.builder.CreateLoad(var->getType()->getPointerElementType(), var);
}
