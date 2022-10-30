#ifndef SYSY_COMPILER_FRONTEND_AST_H
#define SYSY_COMPILER_FRONTEND_AST_H

#include <memory>
#include <vector>
#include <string>
#include <variant>
#include "operator.h"
#include "type.h"

// 使用Memory管理内存，最后统一释放，由于bison对智能指针支持不好，因此使用此解决方案

// 使用struct是因为默认权限为public，可以直接访问成员变量、成员函数

namespace AST {

    struct Base {
        virtual ~Base() = default;
    };

    struct Stmt : Base {};
    struct Expr : Base {};
    struct Decl : Base {};

    ////////////////////////////////////////////////////////////////////////////
    // 编译单元

    struct CompileUnit : Base {
        // 存储：常量、变量声明 或 函数定义
        std::vector<Base *> compileElements;
    };

    ////////////////////////////////////////////////////////////////////////////
    // 常量、变量初始化

    struct InitializerList;
    struct InitializerElement {
        std::variant<Expr *, InitializerList *> element;
    };

    struct InitializerList {
        std::vector<InitializerElement *> elements;
    };

    // 容器类
    struct Array {
        std::string name;
        std::vector<Expr *> size;
    };

    ////////////////////////////////////////////////////////////////////////////
    // 常量、变量声明

    struct ConstVariableDef {
        std::string name;
        // 数组维度，若普通变量则为空，若为数组则存储数组维度
        // 注：维度不一定是字面值常量，可以为int a[10/2];
        std::vector<Expr *> size;
        // 数组初值，若为普通变量，则仅有0个或1个数值
        // 若为数组，则以initializer_list的方式存储，如{1, 2, 3, 4}
        // 注：初始化列表可以嵌套，如{{1, 2}, 3, 4}，此时AST加深一层。也可以不初始化，此时为空指针
        InitializerElement *initVal;
    };

    // 容器类
    struct ConstVariableDefList {
        std::vector<ConstVariableDef *> constVariableDefs;
    };

    struct ConstVariableDecl : Decl {
        // 声明的常量类型，只存储基本类型，如int，float
        Typename type;
        // 存储常量定义，由于一个声明可以定义多个常量，所以使用vector
        // 例 int a = 1, b = 2;，constVariableDef中存储的就是"a = 1"
        std::vector<ConstVariableDef *> constVariableDefs;
    };

    struct VariableDef {
        std::string name;
        std::vector<Expr *> size;
        InitializerElement *initVal;
    };

    // 容器类
    struct VariableDefList {
        std::vector<VariableDef *> variableDefs;
    };

    struct VariableDecl : Decl {
        Typename type;
        std::vector<VariableDef *> variableDefs;
    };

    ////////////////////////////////////////////////////////////////////////////
    // 函数定义

    // TODO: 函数参数是否继承自Base待定
    struct FunctionArg {
        Typename type;
        std::string name;
        // 若为数组，则存储数组维度
        // 注：此时第一维为空指针，从第二维存储数值，例：int a[][3]
        std::vector<Expr *> size;
    };

    // 只是一个容器，用于在parser解析过程中临时存储函数参数
    struct FunctionArgList {
        std::vector<FunctionArg *> arguments;
    };

    struct Block : Base {
        // 存储：常量、变量声明 或 语句
        std::vector<Base *> elements;
    };

    struct FunctionDef : Base {
        Typename returnType;
        std::string name;
        std::vector<FunctionArg *> arguments;
        Block *body;
    };

    ////////////////////////////////////////////////////////////////////////////
    // 语句

    struct LValue {
        std::string name;
        std::vector<Expr *> size;
    };

    struct AssignStmt : Stmt {
        LValue *lValue;
        Expr *rValue;
    };

    struct ExprStmt : Stmt {
        Expr *expr;
    };

    struct NullStmt : Stmt {};

    struct BlockStmt : Stmt {
        std::vector<Base *> elements;
    };

    struct IfStmt : Stmt {
        Expr *condition;
        Stmt *thenStmt;
        // 若存在else，则存储else语句，否则置为空指针
        Stmt *elseStmt;
    };
    struct WhileStmt : Stmt {
        Expr *condition;
        Stmt *body;
    };

    struct BreakStmt : Stmt {};

    struct ContinueStmt : Stmt {};

    struct ReturnStmt : Stmt {
        Expr *expr;
    };

    ////////////////////////////////////////////////////////////////////////////
    // 表达式

    struct UnaryExpr : Expr {
        Operator op;
        Expr *expr;
    };

    struct FunctionParamList {
        std::vector<Expr *> params;
    };

    struct FunctionCallExpr : Expr {
        std::string name;
        std::vector<Expr *> params;
    };

    struct BinaryExpr : Expr {
        Operator op;
        Expr *lhs;
        Expr *rhs;
    };

    struct NumberExpr : Expr {
        Typename type;
        std::string valueStr;
    };

    struct VariableExpr : Expr {
        std::string name;
        std::vector<Expr *> size;
    };
}

// 根节点
namespace AST {
    extern Base *root;
}

#endif //SYSY_COMPILER_FRONTEND_AST_H
