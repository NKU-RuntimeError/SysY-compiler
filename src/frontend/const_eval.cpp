#include <stdexcept>
#include <variant>
#include <numeric>
#include <deque>
#include "symbol_table.h"
#include "mem.h"
#include "AST.h"

static SymbolTable<std::variant<int, float> *> constEvalSymTable;

////////////////////////////////////////////////////////////////////////////////
// helper函数

template<typename Ty>
void constEvalTp(Ty &p) {
    // 确保输入类型正确
    static_assert(std::is_base_of_v<AST::Base, std::remove_pointer_t<Ty>>);
    // 调用子树的constEval进行常量求值
    AST::Base *base = p;
    base->constEval(base);
    p = dynamic_cast<Ty>(base);
    if (!p) {
        throw std::logic_error("constEvalTp: dynamic_cast failed");
    }
}

static Typename getType(const std::variant<int, float> &v) {
    if (std::holds_alternative<int>(v)) {
        return Typename::INT;
    } else {
        return Typename::FLOAT;
    }
}

// 对编译期常量进行类型转换
static std::variant<int, float>
typeFix(std::variant<int, float> v, Typename wantType) {
    // 获得存储类型
    Typename holdType = getType(v);

    if (holdType == Typename::INT && wantType == Typename::FLOAT) {
        return static_cast<float>(std::get<int>(v));
    }
    if (holdType == Typename::FLOAT && wantType == Typename::INT) {
        return static_cast<int>(std::get<float>(v));
    }

    throw std::runtime_error("unexpected cast");
}

// 确保常量初始化列表全部是字面值常量
static void constInitializerAssert(AST::InitializerElement *node) {
    if (std::holds_alternative<AST::Expr *>(node->element)) {
        // 尝试转换到数值表达式，如果失败则抛出异常
        if (!dynamic_cast<AST::NumberExpr *>(std::get<AST::Expr *>(node->element))) {
            throw std::runtime_error("unexpected non-constant initializer");
        }
    } else {
        // 递归进行常量求值Assert
        auto initializerList = std::get<AST::InitializerList *>(node->element);
        for (auto element: initializerList->elements) {
            constInitializerAssert(element);
        }
    }
}

static void initializerTypeFix(AST::InitializerElement *node, Typename wantType) {
    if (std::holds_alternative<AST::Expr *>(node->element)) {
        // 尝试转换到数值表达式
        auto numberExpr = dynamic_cast<AST::NumberExpr *>(std::get<AST::Expr *>(node->element));
        if (!numberExpr) {
            return;
        }

        // 若类型不匹配，进行类型转换
        if (getType(numberExpr->value) != wantType) {
            numberExpr->value = typeFix(numberExpr->value, wantType);
        }
    } else {
        // 递归进行类型转换
        auto initializerList = std::get<AST::InitializerList *>(node->element);
        for (auto element: initializerList->elements) {
            initializerTypeFix(element, wantType);
        }
    }
}

// 检查数组维度，确保数组维度为非负整数
static void constExprCheck(AST::Expr *size) {
    auto numberExpr = dynamic_cast<AST::NumberExpr *>(size);
    if (!numberExpr) {
        throw std::runtime_error("unexpected non-constant array size");
    }
    if (std::get<int>(numberExpr->value) < 0) {
        throw std::runtime_error("array size must be non-negative");
    }
}

static void initializerFlatten(
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
    for (auto flattenElement: initializerList->elements) {
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

static void initializerSplit(AST::InitializerElement *initializerElement, std::deque<int> size) {
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

static void fixNestedInitializer(
        AST::InitializerElement *initializerElement,
        const std::vector<AST::Expr *> &size,
        Typename type
) {
    std::deque<int> sizeDeque;
    // 在维度进行完常量求值后，再进行数组修复，因此可以确保一定是>=0的字面值常量
    for (auto element: size) {
        auto numberExpr = dynamic_cast<AST::NumberExpr *>(element);
        sizeDeque.emplace_back(std::get<int>(numberExpr->value));
    }

    // 将多维数组展开为一维数组
    initializerFlatten(initializerElement, sizeDeque, type);

    // 将数组拆分为多维数组
    initializerSplit(initializerElement, sizeDeque);
}

////////////////////////////////////////////////////////////////////////////////
// 常量求值函数

void AST::CompileUnit::constEval(AST::Base *&root) {
    // 注意这个auto &，由于是引用，所以可以递归修改子树指针
    // 后续代码多次使用auto &，需要注意
    for (auto &compileElement: compileElements) {
        constEvalTp(compileElement);
    }
}

void AST::InitializerElement::constEval(AST::Base *&root) {
    if (std::holds_alternative<Expr *>(element)) {
        constEvalTp(std::get<Expr *>(element));
    } else {
        constEvalTp(std::get<InitializerList *>(element));
    }
}

void AST::InitializerList::constEval(AST::Base *&root) {
    for (auto &element: elements) {
        constEvalTp(element);
    }
}

void AST::ConstVariableDecl::constEval(AST::Base *&root) {
    for (auto def: constVariableDefs) {
        // 对各维度求值
        for (auto &s: def->size) {
            constEvalTp(s);

            // 根据SysY定义：
            // "ConstDef中表示各维长度的ConstExp都必须能在编译时求值到非负整数。"
            constExprCheck(s);
        }

        // 确保初值存在
        // 例：const int a;
        if (!def->initVal) {
            throw std::runtime_error("const variable must be initialized");
        }

        // 修复嵌套数组
        fixNestedInitializer(def->initVal, def->size, type);

        // 尝试对初值求值
        constEvalTp(def->initVal);

        // 确保求值成功
        constInitializerAssert(def->initVal);

        // 对初值进行类型转换
        // 例：float a = 1; 将(int)1转换为(float)1.0
        initializerTypeFix(def->initVal, type);

        // 将普通常量存储在符号表中
        // 注意：不考虑数组常量，数组维度是empty即代表是普通常量
        if (def->size.empty()) {
            // 由于上面已经确保了求值成功，因此在这里numberExpr一定不是空指针
            auto numberExpr = dynamic_cast<AST::NumberExpr *>(
                    std::get<AST::Expr *>(def->initVal->element)
            );

            // 在常量表中插入常量，在后续常量求值中可能会使用
            constEvalSymTable.insert(
                    def->name,
                    Memory::make<std::variant<int, float>>(numberExpr->value)
            );
        }
    }
}

void AST::VariableDecl::constEval(AST::Base *&root) {
    for (auto def: variableDefs) {
        // 对各维度求值
        for (auto &s: def->size) {
            constEvalTp(s);

            // 根据SysY定义：
            // "ConstDef中表示各维长度的ConstExp都必须能在编译时求值到非负整数。"
            constExprCheck(s);
        }

        // 跳过没有初值的变量
        if (!def->initVal) {
            continue;
        }

        // 修复嵌套数组
        fixNestedInitializer(def->initVal, def->size, type);

        // 尝试对初值求值
        // 全局变量需要可编译期求值，因此需要在此尝试求值
        constEvalTp(def->initVal);

        // 对初值进行类型转换
        // 例：float a = 1; 将(int)1转换为(float)1.0
        initializerTypeFix(def->initVal, type);
    }
}

void AST::FunctionArg::constEval(AST::Base *&root) {
    for (auto &s: size) {
        // 跳过第一维度的参数，例：int a[][3]，第一维为nullptr
        if (!s) {
            continue;
        }

        constEvalTp(s);

        // 确保求值成功
        constExprCheck(s);
    }
}

void AST::Block::constEval(AST::Base *&root) {
    for (auto &element: elements) {
        constEvalTp(element);
    }
}

void AST::FunctionDef::constEval(AST::Base *&root) {
    constEvalSymTable.push();

    for (auto &argument: arguments) {
        constEvalTp(argument);
    }
    constEvalTp(body);

    constEvalSymTable.pop();
}

void AST::AssignStmt::constEval(AST::Base *&root) {
    // 什么也不做
}

void AST::ExprStmt::constEval(AST::Base *&root) {
    // 什么也不做
}

void AST::NullStmt::constEval(AST::Base *&root) {
    // 什么也不做
}

void AST::BlockStmt::constEval(AST::Base *&root) {
    constEvalSymTable.push();

    for (auto &element: elements) {
        constEvalTp(element);
    }

    constEvalSymTable.pop();
}

void AST::IfStmt::constEval(AST::Base *&root) {
    constEvalTp(thenStmt);
    if (elseStmt) {
        constEvalTp(elseStmt);
    }
}

void AST::WhileStmt::constEval(AST::Base *&root) {
    constEvalTp(body);
}

void AST::BreakStmt::constEval(AST::Base *&root) {
    // 什么也不做
}

void AST::ContinueStmt::constEval(AST::Base *&root) {
    // 什么也不做
}

void AST::ReturnStmt::constEval(AST::Base *&root) {
    // 什么也不做
}

void AST::UnaryExpr::constEval(AST::Base *&root) {
    // 尝试对子表达式求值
    constEvalTp(expr);

    // 去除正号
    // 由于表达式的值不变，因此直接提升即可
    if (op == Operator::ADD) {
        root = expr;
        return;
    }

    // 计算负号
    if (op == Operator::SUB) {
        auto numberExpr = dynamic_cast<AST::NumberExpr *>(expr);
        if (!numberExpr) {
            return;
        }

        if (std::holds_alternative<int>(numberExpr->value)) {
            root = Memory::make<AST::NumberExpr>(-std::get<int>(numberExpr->value));
        } else {
            root = Memory::make<AST::NumberExpr>(-std::get<float>(numberExpr->value));
        }
    }

    // 不考虑条件表达式
}

void AST::FunctionCallExpr::constEval(AST::Base *&root) {
    // 什么也不做
}

static std::tuple<std::variant<int, float>, std::variant<int, float>, Typename>
binaryExprTypeFix(AST::NumberExpr *L, AST::NumberExpr *R) {

    // 获得该节点的计算类型
    // 取max是因为在Typename中编号按照类型优先级排列，越大优先级越高
    // 对于每个二元运算，我们希望类型向高处转换，保证计算精度
    Typename nodeType = static_cast<Typename>(std::max(
            static_cast<int>(getType(L->value)),
            static_cast<int>(getType(R->value))
    ));

    // 按需进行类型转换
    auto Lv = L->value;
    auto Rv = R->value;
    if (getType(Lv) != nodeType) {
        Lv = typeFix(Lv, nodeType);
    }
    if (getType(Rv) != nodeType) {
        Rv = typeFix(Rv, nodeType);
    }

    // 返回转换结果、计算类型
    return {Lv, Rv, nodeType};
}

void AST::BinaryExpr::constEval(AST::Base *&root) {
    constEvalTp(lhs);
    constEvalTp(rhs);

    // 若左右子表达式均为常量，则进行计算，否则直接返回
    auto numberExprLhs = dynamic_cast<AST::NumberExpr *>(lhs);
    auto numberExprRhs = dynamic_cast<AST::NumberExpr *>(rhs);
    if (!numberExprLhs || !numberExprRhs) {
        return;
    }

    // 进行隐式类型转换
    auto [L, R, nodeType] = binaryExprTypeFix(
            numberExprLhs,
            numberExprRhs
    );

    // 尝试对子表达式求值
    switch (op) {
        case Operator::ADD: {
            if (nodeType == Typename::INT) {
                root = Memory::make<AST::NumberExpr>(std::get<int>(L) + std::get<int>(R));
                return;
            }
            if (nodeType == Typename::FLOAT) {
                root = Memory::make<AST::NumberExpr>(std::get<float>(L) + std::get<float>(R));
                return;
            }
            break;
        }
        case Operator::SUB: {
            if (nodeType == Typename::INT) {
                root = Memory::make<AST::NumberExpr>(std::get<int>(L) - std::get<int>(R));
                return;
            }
            if (nodeType == Typename::FLOAT) {
                root = Memory::make<AST::NumberExpr>(std::get<float>(L) - std::get<float>(R));
                return;
            }
            break;
        }
        case Operator::MUL: {
            if (nodeType == Typename::INT) {
                root = Memory::make<AST::NumberExpr>(std::get<int>(L) * std::get<int>(R));
                return;
            }
            if (nodeType == Typename::FLOAT) {
                root = Memory::make<AST::NumberExpr>(std::get<float>(L) * std::get<float>(R));
                return;
            }
            break;
        }
        case Operator::DIV: {
            if (nodeType == Typename::INT) {
                root = Memory::make<AST::NumberExpr>(std::get<int>(L) / std::get<int>(R));
                return;
            }
            if (nodeType == Typename::FLOAT) {
                root = Memory::make<AST::NumberExpr>(std::get<float>(L) / std::get<float>(R));
                return;
            }
            break;
        }
        case Operator::MOD: {
            if (nodeType == Typename::INT) {
                root = Memory::make<AST::NumberExpr>(std::get<int>(L) % std::get<int>(R));
                return;
            }
            break;
        }
    }
    throw std::runtime_error("binary operator consteval failed");
}

void AST::NumberExpr::constEval(AST::Base *&root) {
    // 什么也不做
}

void AST::VariableExpr::constEval(AST::Base *&root) {
    // 从符号表中查找编译期常量
    std::variant<int, float> *pValue = constEvalSymTable.tryLookup(name);
    if (!pValue) {
        return;
    }
    auto &value = *pValue;

    // 将根节点转换为字面值常量
    if (std::holds_alternative<int>(value)) {
        root = Memory::make<AST::NumberExpr>(std::get<int>(value));
    } else {
        root = Memory::make<AST::NumberExpr>(std::get<float>(value));
    }
}
