#ifndef SYSY_COMPILER_FRONTEND_AST_H
#define SYSY_COMPILER_FRONTEND_AST_H

#include <memory>
#include <vector>
#include <string>
#include "type.h"

// 使用Memory管理内存，最后统一释放，由于bison对智能指针支持不好，因此使用此解决方案

// 使用struct是因为默认权限为public，可以直接访问成员变量、成员函数

// TODO: 增加更多基类，如Expression、Statement，更清晰地表达继承关系
struct ConstVariableDef;
struct VariableDef;
struct FunctionArgument;

namespace AST {
    struct Base {
        virtual ~Base() = default;
    };

    struct Expr : Base {

    };
    struct CompileUnit : Base {
        // 存储：常量、变量声明 或 函数定义
        std::vector<Base *> compileElements;
    };

    struct ConstVariableDecl : Base {
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
    struct VariableDecl : Base {
        Typename type;
        std::vector<VariableDef *> variableDefs;
    };

    struct VariableDef : Base {
        std::string name;
        std::vector<Base *> size;
        std::vector<Base *> values;
    };

    struct FunctionDef : Base {
        Typename returnType;
        std::string name;
        std::vector<FunctionArgument *> arguments;
        Base *body;
    };

    struct FunctionArgument : Base {
        Typename type;
        std::string name;
        // 若为数组，则存储数组维度
        // 注：此时第一维为空指针，从第二维存储数值，例：int a[][3]
        std::vector<Base *> size;
    };

    struct Block : Base {
        // 存储：常量、变量声明 或 语句
        std::vector<Base *> blockElements;
    };

    struct Assign : Base {
        // 左值
        Base *lvalue;
        // 右值
        Base *rvalue;
    };

    struct If : Base {
        Base *condition;
        Base *thenStmt;
        // 若存在else，则存储else语句，否则置为空指针
        Base *elseStmt;
    };
}

// TODO: 实现其他AST

// 根节点
namespace AST {
    extern Base *root;
}

#endif //SYSY_COMPILER_FRONTEND_AST_H
