#ifndef SYSY_COMPILER_FRONTEND_AST_H
#define SYSY_COMPILER_FRONTEND_AST_H

// 使用struct是因为默认权限为public，可以直接访问成员变量、成员函数

struct BaseAST {
    virtual ~BaseAST() = default;
};

#endif //SYSY_COMPILER_FRONTEND_AST_H
