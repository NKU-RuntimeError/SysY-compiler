#include <stdexcept>
#include <variant>
#include "symbol_table.h"
#include "mem.h"
#include "AST.h"

static SymbolTable<std::variant<int, float> *> constEvalSymTable;

template<typename Ty>
void constEvalTp(Ty &p) {
    // 确保输入类型正确
    static_assert(std::is_base_of_v<AST::Base, std::remove_pointer_t<Ty>>);
    // 调用子树的constEval进行常量求值
    AST::Base *base = p;
    base->constEval(base);
    p = dynamic_cast<Ty>(base);
}

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

static Typename getType(const std::variant<int, float> &v) {
    if (std::holds_alternative<int>(v)) {
        return Typename::INT;
    } else {
        return Typename::FLOAT;
    }
}

static std::variant<int, float>
typeFix(std::variant<int, float> v, Typename wantType) {
    // 获得存储类型
    Typename holdType = getType(v);

    // 仅允许进行以下一种类型的转换：
    // 参考SysY语言定义
    // "数组元素初值类型应与数组元素声明类型一致，例如整型数组初值列表中不能出现浮点型元素；
    // 但是浮点型数组的初始化列表中可以出现整型常量或整型常量表达式"
    if (holdType == Typename::INT && wantType == Typename::FLOAT) {
        return static_cast<float>(std::get<int>(v));
    }

    throw std::runtime_error("unexpected cast in initializer");
}

// 确保常量初始化列表全部是字面值常量
static void constInitializerAssert(AST::InitializerElement *node) {
    if (std::holds_alternative<AST::Expr *>(node->element)) {
        // 尝试转换到数值表达式，如果失败则抛出异常
        [[maybe_unused]]
        auto ptr = dynamic_cast<AST::NumberExpr *>(std::get<AST::Expr *>(node->element));
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
        try {
            // 尝试转换到数值表达式
            auto numberExpr = dynamic_cast<AST::NumberExpr *>(
                    std::get<AST::Expr *>(node->element)
            );
            // 若类型不匹配，进行类型转换
            if (numberExpr->type != wantType) {
                numberExpr->type = wantType;
                numberExpr->value = typeFix(numberExpr->value, wantType);
            }
        } catch (std::bad_cast &) {
        }
    } else {
        // 递归进行类型转换
        auto initializerList = std::get<AST::InitializerList *>(node->element);
        for (auto element: initializerList->elements) {
            initializerTypeFix(element, wantType);
        }
    }
}

void AST::ConstVariableDecl::constEval(AST::Base *&root) {
    for (auto def: constVariableDefs) {
        // 对各维度求值
        for (auto &s: def->size) {
            constEvalTp(s);

            // 根据SysY定义：
            // "ConstDef中表示各维长度的ConstExp都必须能在编译时求值到非负整数。"
            auto numberExpr = dynamic_cast<AST::NumberExpr *>(s);
            if (std::get<int>(numberExpr->value) < 0) {
                throw std::runtime_error("array size must be non-negative");
            }
        }

        // 确保初值存在
        // 例：const int a;
        if (!def->initVal) {
            throw std::runtime_error("const variable must be initialized");
        }

        // 尝试对初值求值
        constEvalTp(def->initVal);

        // 确保求值成功
        constInitializerAssert(def->initVal);

        // 对初值进行类型转换
        // 例：const float a = 1;
        initializerTypeFix(def->initVal, type);

        // 将普通常量存储在符号表中
        // 注意：不考虑数组常量
        if (def->size.empty()) {
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
            auto numberExpr = dynamic_cast<AST::NumberExpr *>(s);
            if (std::get<int>(numberExpr->value) < 0) {
                throw std::runtime_error("array size must be non-negative");
            }
        }

        // 跳过没有初值的变量
        if (!def->initVal) {
            continue;
        }

        // 尝试对初值求值
        // 全局变量需要可编译期求值，因此需要在此尝试求值
        constEvalTp(def->initVal);

        // 对初值进行类型转换
        // 例：float a = 1;
        initializerTypeFix(def->initVal, type);
    }
}

void AST::FunctionArg::constEval(AST::Base *&root) {
    for (auto &s: size) {
        constEvalTp(s);
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

static AST::NumberExpr *getNumberExpr(Typename type, std::variant<int, float> value) {
    auto ptr = Memory::make<AST::NumberExpr>();
    ptr->type = type;
    ptr->value = value;
    return ptr;
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
        try {
            auto numberExpr = dynamic_cast<AST::NumberExpr *>(expr);
            if (numberExpr->type == Typename::INT) {
                root = getNumberExpr(
                        Typename::INT,
                        -std::get<int>(numberExpr->value)
                );
            }
            if (numberExpr->type == Typename::FLOAT) {
                root = getNumberExpr(
                        Typename::FLOAT,
                        -std::get<float>(numberExpr->value)
                );
            }
        } catch (std::bad_cast &) {
        }
        return;
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
            static_cast<int>(L->type),
            static_cast<int>(R->type)
    ));

    // 按需进行类型转换
    auto Lv = L->value;
    auto Rv = R->value;
    if (L->type != nodeType) {
        Lv = typeFix(Lv, nodeType);
    }
    if (R->type != nodeType) {
        Rv = typeFix(Rv, nodeType);
    }

    // 返回转换结果、计算类型
    return {Lv, Rv, nodeType};
}

void AST::BinaryExpr::constEval(AST::Base *&root) {
    constEvalTp(lhs);
    constEvalTp(rhs);

    try {
        // 类型转换
        auto [L, R, nodeType] = binaryExprTypeFix(
                dynamic_cast<AST::NumberExpr *>(lhs),
                dynamic_cast<AST::NumberExpr *>(rhs)
        );

        // 尝试对子表达式求值
        switch (op) {
            case Operator::ADD: {
                if (nodeType == Typename::INT) {
                    root = getNumberExpr(
                            Typename::INT,
                            std::get<int>(L) + std::get<int>(R)
                    );
                    return;
                }
                if (nodeType == Typename::FLOAT) {
                    root = getNumberExpr(
                            Typename::FLOAT,
                            std::get<float>(L) + std::get<float>(R)
                    );
                    return;
                }
            }
            case Operator::SUB: {
                if (nodeType == Typename::INT) {
                    root = getNumberExpr(
                            Typename::INT,
                            std::get<int>(L) - std::get<int>(R)
                    );
                    return;
                }
                if (nodeType == Typename::FLOAT) {
                    root = getNumberExpr(
                            Typename::FLOAT,
                            std::get<float>(L) - std::get<float>(R)
                    );
                    return;
                }
            }
            case Operator::MUL: {
                if (nodeType == Typename::INT) {
                    root = getNumberExpr(
                            Typename::INT,
                            std::get<int>(L) * std::get<int>(R)
                    );
                    return;
                }
                if (nodeType == Typename::FLOAT) {
                    root = getNumberExpr(
                            Typename::FLOAT,
                            std::get<float>(L) * std::get<float>(R)
                    );
                    return;
                }
            }
            case Operator::DIV: {
                if (nodeType == Typename::INT) {
                    root = getNumberExpr(
                            Typename::INT,
                            std::get<int>(L) / std::get<int>(R)
                    );
                    return;
                }
                if (nodeType == Typename::FLOAT) {
                    root = getNumberExpr(
                            Typename::FLOAT,
                            std::get<float>(L) / std::get<float>(R)
                    );
                    return;
                }
            }
            case Operator::MOD: {
                if (nodeType == Typename::INT) {
                    root = getNumberExpr(
                            Typename::INT,
                            std::get<int>(L) % std::get<int>(R)
                    );
                    return;
                }
                if (nodeType == Typename::FLOAT) {
                    throw std::runtime_error("float type cannot use mod operator");
                }
            }
        }
    } catch (std::bad_cast &) {
        // 什么也不做
    }
}

void AST::NumberExpr::constEval(AST::Base *&root) {
    // 什么也不做
}

void AST::VariableExpr::constEval(AST::Base *&root) {
    std::variant<int, float> *pValue = constEvalSymTable.tryLookup(name);
    if (!pValue) {
        return;
    }
    auto &value = *pValue;
    if (std::holds_alternative<int>(value)) {
        root = getNumberExpr(Typename::INT, std::get<int>(value));
        return;
    }
    if (std::holds_alternative<float>(value)) {
        root = getNumberExpr(Typename::FLOAT, std::get<float>(value));
        return;
    }
}

