#ifndef SYSY_COMPILER_FRONTEND_AST_H
#define SYSY_COMPILER_FRONTEND_AST_H

#include <memory>
#include <vector>
#include <string>
#include "type.h"

// 使用Memory管理内存，最后统一释放，由于bison对智能指针支持不好，因此使用此解决方案

// 使用struct是因为默认权限为public，可以直接访问成员变量、成员函数

// TODO: 增加更多基类，如Expression、Statement，更清晰地表达继承关系

namespace AST {
    struct Base {
        virtual ~Base() = default;
    };
    struct Stmt :Base {

    };
    struct Expr : Base {

    };
    struct ConstExpr : Expr{

    };
    struct CompileUnit : Base {
        // 存储：常量、变量声明 或 函数定义
        std::vector<Base *> compileElements;
    };

    struct Decl;
    struct FunctionDef;
    struct CompileElement : Base {
        // type为0说明是decl，为1说明是Funcdef
        int type;
        Decl * decl;
        FunctionDef *functionDef;
    };
    // 常量和变量声明的基类
    struct Decl : Base {

    };
    struct ConstVariableDef;
    struct ConstVariableDecl : Decl {
        // 声明的常量类型，只存储基本类型，如int，float
        Typename type;
        // 存储常量定义，由于一个声明可以定义多个常量，所以使用vector
        // 例 int a = 1, b = 2;，constVariableDef中存储的就是"a = 1"
        std::vector<ConstVariableDef *> constVariableDefs;
    };

    struct ConstVariableDef : Base {
        std::string name;
        // 数组维度，若普通变量则为空，若为数组则存储数组维度
        // 注：维度不一定是字面值常量，可以为int a[10/2];
        std::vector<Base *> size;
        // 数组初值，若为普通变量，则仅有0个或1个数值
        // 若为数组，则以initializer_list的方式存储，如{1, 2, 3, 4}
        // 注：初始化列表可以嵌套，如{{1, 2}, 3, 4}，此时AST加深一层。也可以不初始化，该数组大小为0
        std::vector<Base *> values;
    };
     // 同ConstVariableDecl和ConstVariableDef
    struct VariableDef;
    struct VariableDecl : Decl {
         Typename type;
         std::vector<VariableDef *> variableDefs;
    };
    struct VariableDef : Base {
        std::string name;
        std::vector<Base *> size;
        std::vector<Base *> values;
    };

    // 各类list，只包含一个向量
    struct List : Base {
    };
    struct ConstVarDefList : List{
        std::vector<ConstVariableDef *> defs;
    };
    struct VarDefList : List{
        std::vector<VariableDef *> defs;
    };
    struct ConstVarValueList : List{
        std::vector<Base *> values;
    };
    struct ConstVarElement : Base{
        // type为0表示是expr，为1表示是list
        int type;
        ConstExpr *expr;
        ConstVarValueList *list;
    };
    struct VarValueList : List{
        std::vector<Base *> values;
    };
    struct VarElement : Base{
        // type为0表示是expr，为1表示是list
        int type;
        Expr *expr;
        VarValueList *list;
    };
    struct ParamList : List {
        std::vector<Expr *>exprs;
    };
    // 数组，包括名称和维度
    struct Array : Base {
        std::string name;
        std::vector<Base *> size;
    };

    struct FunctionArgument;
    struct Block;
    struct FunctionDef : Base {
        Typename returnType;
        std::string name;
        std::vector<FunctionArgument *> arguments;
        Block *body;
    };
    struct FunctionArgument : Base {
        Typename type;
        std::string name;
        // 若为数组，则存储数组维度
        // 注：此时第一维为空指针，从第二维存储数值，例：int a[][3]
        std::vector<Base *> size;
    };
    struct FunctionArgList : List {
        std::vector<FunctionArgument *> arguments;
    };

    struct Block : Base {
        // 存储：常量、变量声明 或 语句
        std::vector<Base *> blockElements;
    };
    struct BlockElement : Base {
        // type为0说明是decl，为1说明是stmt
        int type;
        Decl *decl;
        Stmt *stmt;
    };
    struct AssignStmt : Stmt {
        // 左值
        Base *lvalue;
        // 右值
        Base *rvalue;
    };
    struct ExprStmt : Stmt {
        Expr *expr;
    };
    struct BlockStmt : Stmt {
        Block *block;
    };
    struct LogicalExpr;
    struct IfStmt : Stmt {
        LogicalExpr *condition;
        Stmt *thenStmt;
        // 若存在else，则存储else语句，否则置为空指针
        Stmt *elseStmt;
    };
    struct WhileStmt : Stmt {
        Base *condition;
        Stmt *stmt;
    };
    struct BreakStmt : Stmt {
    };
    struct ContiStmt : Stmt {
    };
    struct ReturnStmt : Stmt {
        Expr *expr;
    };
    struct Number;
    struct PrimaryExpr : Expr {
        // type为0说明是expr，为1说明是lval，为2说明是number
        int type;
        Expr *expr;
        Array *lval;
        Number *number;
    };
    struct UnaryExpr : Expr {
        // dectype表示是primaryExpr表达式还是函数返回的Expr
        int decType;
        // op为0表示正，为1表示负，为2表示非
        int op;
        PrimaryExpr *pExpr;
        std::string *name;
        ParamList *paramList;
    };
    struct Number : Base {
        // type为0表示int，为1表示float
        int type;
        std::string *str;
    };
    struct AsmddExpr : ConstExpr {
        // 0代表Add，1代表Sub，2代表Mul，3代表Div,4代表Mod
        int op;
        Expr *lexpr;
        Expr *rexpr;
    };
    struct RelationExpr : ConstExpr {
        // 0代表等于，1代表不等于，2代表小于，3代表大于，4代表小于等于，5代表大于等于
        int op;
        Expr *lexpr;
        Expr *rexpr;
    };
    struct LogicalExpr : ConstExpr {
        // 0代表逻辑与，1代表逻辑或
        int op;
        Expr *lexpr;
        Expr *rexpr;
    };

}

// TODO: 实现其他AST

// 根节点
namespace AST {
    extern Base *root;
}

#endif //SYSY_COMPILER_FRONTEND_AST_H
