#include <variant>
#include <stdexcept>
#include <deque>
#include <numeric>
#include "mem.h"
#include "type.h"
#include "const_eval_helper.h"

using namespace ConstEvalHelper;

// 对编译期常量进行类型转换
std::variant<int, float>
ConstEvalHelper::typeFix(
        std::variant<int, float> v,
        Typename wantType
) {
    // 获得存储类型
    Typename holdType = TypeSystem::from(v);

    if (holdType == Typename::INT && wantType == Typename::FLOAT) {
        return static_cast<float>(std::get<int>(v));
    }
    if (holdType == Typename::FLOAT && wantType == Typename::INT) {
        return static_cast<int>(std::get<float>(v));
    }

    throw std::runtime_error("unexpected cast");
}

// 确保常量初始化列表全部是字面值常量
void
ConstEvalHelper::constInitializerAssert(
        AST::InitializerElement *node
) {
    if (std::holds_alternative<AST::Expr *>(node->element)) {
        // 尝试转换到数值表达式，如果失败则抛出异常
        if (!dynamic_cast<AST::NumberExpr *>(std::get<AST::Expr *>(node->element))) {
            throw std::runtime_error("unexpected non-constant initializer");
        }
    } else {
        // 递归进行常量求值Assert
        auto initializerList = std::get<AST::InitializerList *>(node->element);
        for (AST::InitializerElement* element: initializerList->elements) {
            constInitializerAssert(element);
        }
    }
}

void
ConstEvalHelper::initializerTypeFix(
        AST::InitializerElement *node,
        Typename wantType
) {
    if (std::holds_alternative<AST::Expr *>(node->element)) {
        // 尝试转换到数值表达式
        auto numberExpr = dynamic_cast<AST::NumberExpr *>(std::get<AST::Expr *>(node->element));
        if (!numberExpr) {
            return;
        }

        // 若类型不匹配，进行类型转换
        if (TypeSystem::from(numberExpr->value) != wantType) {
            numberExpr->value = typeFix(numberExpr->value, wantType);
        }
    } else {
        // 递归进行类型转换
        auto initializerList = std::get<AST::InitializerList *>(node->element);
        for (AST::InitializerElement* element: initializerList->elements) {
            initializerTypeFix(element, wantType);
        }
    }
}

// 检查数组维度，确保数组维度为非负整数
void
ConstEvalHelper::constExprCheck(
        AST::Expr *size
) {
    auto numberExpr = dynamic_cast<AST::NumberExpr *>(size);
    if (!numberExpr) {
        throw std::runtime_error("unexpected non-constant array size");
    }
    if (std::get<int>(numberExpr->value) < 0) {
        throw std::runtime_error("array size must be non-negative");
    }
}

// 展开成一维并补零
void
ConstEvalHelper::initializerFlatten(
        AST::InitializerElement *initializerElement,
        std::deque<int> size,
        Typename type
) {
    // 递归出口，到达最底层表达式
    if (std::holds_alternative<AST::Expr *>(initializerElement->element)) {
        return;
    }

    // 中间节点，尝试进行提升
    if (size.empty()) {
        throw std::runtime_error("nested initializer list is too deep");
    }

    // 计算当前维度需要多少个数
    // 例：int[4][2] -> fullSize = 8
    int fullSize = std::reduce(
            size.begin(),
            size.end(),
            1,
            std::multiplies<>()
    );

    auto initializerList = std::get<AST::InitializerList *>(
            initializerElement->element
    );

    // 用于临时存储当前层展开的结果
    std::vector<AST::InitializerElement *> elements;

    // 弹出第一个维度，递归向下做flatten
    size.pop_front();
    for (AST::InitializerElement* flattenElement: initializerList->elements) {
        initializerFlatten(flattenElement, size, type);

        // 区分 单个元素/flatten列表 两种情况，加到当前层的临时列表中
        if (std::holds_alternative<AST::Expr *>(flattenElement->element)) {
            elements.emplace_back(flattenElement);
        } else {
            auto flattenList = std::get<AST::InitializerList *>(
                    flattenElement->element
            );
            elements.insert(
                    elements.end(),
                    flattenList->elements.begin(),
                    flattenList->elements.end()
            );
        }
    }

    // 如果当前层展开的结果数量超过了应有数量，则报错
    if (elements.size() > fullSize) {
        throw std::runtime_error("initializer overflow");
    }

    // 如果当前层展开的结果数量不足，则用0填充
    for (size_t i = elements.size(); i < fullSize; i++) {
        auto ptr = Memory::make<AST::InitializerElement>();
        if (type == Typename::INT) {
            ptr->element = Memory::make<AST::NumberExpr>(0);
        } else {
            ptr->element = Memory::make<AST::NumberExpr>(0.f);
        }
        elements.emplace_back(ptr);
    }

    // 将展开结果存储到当前节点中
    initializerList->elements = elements;
}

// 将展平的一维重新拆分为多维
void
ConstEvalHelper::initializerSplit(
        AST::InitializerElement *initializerElement,
        std::deque<int> size
) {
    // 递归出口，到达最底层表达式
    if (size.size() <= 1) {
        return;
    }

    // 例：int[4][3][2] ->
    //     currSize = 4
    //     fullSize = 24
    //     step     = 6
    int currSize = size.front();
    int fullSize = std::reduce(
            size.begin(),
            size.end(),
            1,
            std::multiplies<>()
    );
    int step = fullSize / currSize;

    auto initializerList = std::get<AST::InitializerList *>(
            initializerElement->element
    );

    // 创建临时数组，保存当前层的每个拆分
    std::vector<AST::InitializerElement *> elements;

    size.pop_front();
    for (int i = 0; i < fullSize; i += step) {

        // 构造拆分的列表
        auto newElement = Memory::make<AST::InitializerElement>();
        auto newList = Memory::make<AST::InitializerList>();
        newList->elements = std::vector<AST::InitializerElement *>(
                initializerList->elements.begin() + i,
                initializerList->elements.begin() + i + step
        );
        newElement->element = newList;

        // 递归向下拆分
        initializerSplit(newElement, size);

        // 保存在临时空间中
        elements.emplace_back(newElement);
    }

    initializerList->elements = elements;
}

// 调用展开和拆分
void
ConstEvalHelper::fixNestedInitializer(
        AST::InitializerElement *initializerElement,
        const std::vector<AST::Expr *> &size,
        Typename type
) {
    std::deque<int> sizeDeque;
    // 在维度进行完常量求值后，再进行数组修复，因此可以确保一定是>=0的字面值常量
    for (AST::Expr* element: size) {
        auto numberExpr = dynamic_cast<AST::NumberExpr *>(element);
        sizeDeque.emplace_back(std::get<int>(numberExpr->value));
    }

    // 将多维数组展开为一维数组
    initializerFlatten(initializerElement, sizeDeque, type);

    // 将数组拆分为多维数组
    initializerSplit(initializerElement, sizeDeque);
}
