#ifndef SYSY_COMPILER_FRONTEND_CONST_EVAL_HELPER_H
#define SYSY_COMPILER_FRONTEND_CONST_EVAL_HELPER_H

#include <type_traits>
#include <stdexcept>
#include "AST.h"

namespace ConstEvalHelper {

    // 常量求值函数，以p为根节点，向下递归调用子节点的constEval函数，并修改子树的根节点
    // Ty需要继承自AST::Base
    // 有了C++20的concept后，写起来会优雅一些
    template <typename Ty>
    using DerivedFromBase = std::enable_if_t<std::is_base_of_v<AST::Base, std::decay_t<Ty>>>;

    template<typename Ty,
            typename = DerivedFromBase<Ty>>
    void constEvalHelper(Ty *&p) {
        // 调用子树的constEval进行常量求值
        // 必须以Base为中转进行类型转换，不然会报错
        AST::Base *base = p;
        base->constEval(base);
        p = static_cast<Ty *>(base);
    }

    // 对常量进行类型修正，支持 int->float 和 float->int
    std::variant<int, float>
    typeFix(
            std::variant<int, float> v,
            Typename wantType
    );

    // 确保常量初始化列表全部是字面值常量
    void
    constInitializerAssert(
            AST::InitializerElement *node
    );

    // 初始化列表初值类型修复，只处理字面值常量，不处理运行时类型转换
    void
    initializerTypeFix(
            AST::InitializerElement *node,
            Typename wantType
    );

    // 检查数组维度，确保数组维度为非负整数
    void
    constExprCheck(
            AST::Expr *size
    );


    // 数组展开，展开成一维数组，并且完成补0工作
    void
    initializerFlatten(
            AST::InitializerElement *initializerElement,
            std::deque<int> size,
            Typename type
    );

    // 数组拆分，按照size拆分成多维数组
    void
    initializerSplit(
            AST::InitializerElement *initializerElement,
            std::deque<int> size
    );

    // 调用flatten和split，完成数组的展开和拆分，实现数组规整化及补0操作
    void
    fixNestedInitializer(
            AST::InitializerElement *initializerElement,
            const std::vector<AST::Expr *> &size,
            Typename type
    );

}

#endif //SYSY_COMPILER_FRONTEND_CONST_EVAL_HELPER_H