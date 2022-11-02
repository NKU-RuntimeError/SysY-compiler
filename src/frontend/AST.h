#ifndef SYSY_COMPILER_FRONTEND_AST_H
#define SYSY_COMPILER_FRONTEND_AST_H

#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <llvm/Support/JSON.h>
#include <llvm/IR/Value.h>
#include "operator.h"
#include "type.h"

// 使用Memory管理内存，最后统一释放，由于bison对智能指针支持不好，因此使用此解决方案

// 使用struct是因为默认权限为public，可以直接访问成员变量、成员函数

namespace AST {

    // 为了简化继承关系，我们将所有子类可能会实现的方法放在Base中
    // 子类可以选择性实现这些方法
    // 注意：这不是一个好的设计，只是为了简化代码
    struct Base {
        virtual llvm::json::Value toJSON() {
            throw std::logic_error("not implemented");
        }

        virtual llvm::Value *codeGen() {
            throw std::logic_error("not implemented");
        }

        virtual ~Base() = default;
    };

    struct Stmt : Base {
    };
    struct Expr : Base {
    };
    struct Decl : Base {
    };

    ////////////////////////////////////////////////////////////////////////////
    // 编译单元

    struct CompileUnit : Base {
        // 存储：常量、变量声明 或 函数定义
        std::vector<Base *> compileElements;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    ////////////////////////////////////////////////////////////////////////////
    // 常量、变量初始化

    struct InitializerList;

    // 容器类
    struct InitializerElement : Base {
        std::variant<Expr *, InitializerList *> element;

        llvm::json::Value toJSON() override;
    };

    // 容器类
    struct InitializerList : Base {
        std::vector<InitializerElement *> elements;

        llvm::json::Value toJSON() override;
    };

    // 容器类，仅在构造AST中作为临时容器使用
    struct Array {
        std::string name;
        std::vector<Expr *> size;
    };

    ////////////////////////////////////////////////////////////////////////////
    // 常量、变量声明

    // 容器类
    struct ConstVariableDef : Base {
        std::string name;
        // 数组维度，若普通变量则为空，若为数组则存储数组维度
        // 注：维度不一定是字面值常量，可以为int a[10/2];
        std::vector<Expr *> size;
        // 数组初值，若为普通变量，则仅有0个或1个数值
        // 若为数组，则以initializer_list的方式存储，如{1, 2, 3, 4}
        // 注：初始化列表可以嵌套，如{{1, 2}, 3, 4}，此时AST加深一层。也可以不初始化，此时为空指针
        InitializerElement *initVal;

        llvm::json::Value toJSON() override;
    };

    // 容器类，仅在构造AST中作为临时容器使用
    struct ConstVariableDefList {
        std::vector<ConstVariableDef *> constVariableDefs;
    };

    struct ConstVariableDecl : Decl {
        // 声明的常量类型，只存储基本类型，如int，float
        Typename type;
        // 存储常量定义，由于一个声明可以定义多个常量，所以使用vector
        // 例 int a = 1, b = 2;，constVariableDef中存储的就是"a = 1"
        std::vector<ConstVariableDef *> constVariableDefs;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    // 容器类
    struct VariableDef : Base {
        std::string name;
        std::vector<Expr *> size;
        InitializerElement *initVal;

        llvm::json::Value toJSON() override;
    };

    // 容器类，仅在构造AST中作为临时容器使用
    struct VariableDefList {
        std::vector<VariableDef *> variableDefs;
    };

    struct VariableDecl : Decl {
        Typename type;
        std::vector<VariableDef *> variableDefs;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    ////////////////////////////////////////////////////////////////////////////
    // 函数定义

    // 容器类
    struct FunctionArg : Base {
        Typename type;
        std::string name;
        // 若为数组，则存储数组维度
        // 注：此时第一维为空指针，从第二维存储数值，例：int a[][3]
        std::vector<Expr *> size;

        llvm::json::Value toJSON() override;
    };

    // 容器类，仅在构造AST中作为临时容器使用
    struct FunctionArgList {
        std::vector<FunctionArg *> arguments;
    };

    struct Block : Base {
        // 存储：常量、变量声明 或 语句
        std::vector<Base *> elements;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    struct FunctionDef : Base {
        Typename returnType;
        std::string name;
        std::vector<FunctionArg *> arguments;
        Block *body;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    ////////////////////////////////////////////////////////////////////////////
    // 语句

    // 容器类
    struct LValue : Base {
        std::string name;
        std::vector<Expr *> size;

        llvm::json::Value toJSON() override;
    };

    struct AssignStmt : Stmt {
        LValue *lValue;
        Expr *rValue;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    struct ExprStmt : Stmt {
        Expr *expr;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    struct NullStmt : Stmt {

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    struct BlockStmt : Stmt {
        std::vector<Base *> elements;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    struct IfStmt : Stmt {
        Expr *condition;
        Stmt *thenStmt;
        // 若存在else，则存储else语句，否则置为空指针
        Stmt *elseStmt;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    struct WhileStmt : Stmt {
        Expr *condition;
        Stmt *body;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    struct BreakStmt : Stmt {

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    struct ContinueStmt : Stmt {

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    struct ReturnStmt : Stmt {
        Expr *expr;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    ////////////////////////////////////////////////////////////////////////////
    // 表达式

    struct UnaryExpr : Expr {
        Operator op;
        Expr *expr;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    // 容器类，仅在构造AST中作为临时容器使用
    struct FunctionParamList {
        std::vector<Expr *> params;
    };

    struct FunctionCallExpr : Expr {
        std::string name;
        std::vector<Expr *> params;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    struct BinaryExpr : Expr {
        Operator op;
        Expr *lhs;
        Expr *rhs;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    struct NumberExpr : Expr {
        Typename type;
        std::variant<int, float> value;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };

    struct VariableExpr : Expr {
        std::string name;
        std::vector<Expr *> size;

        llvm::json::Value toJSON() override;

        llvm::Value *codeGen() override;
    };
}

// 根节点
namespace AST {
    extern Base *root;

    void show();
}

#endif //SYSY_COMPILER_FRONTEND_AST_H
