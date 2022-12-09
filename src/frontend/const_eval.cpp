#include <stdexcept>
#include <variant>
#include <numeric>
#include <deque>
#include <type_traits>
#include "symbol_table.h"
#include "mem.h"
#include "AST.h"
#include "const_eval_helper.h"

using namespace ConstEvalHelper;

// 常量求值符号表，仅存储普通常量，不存储数组常量
static SymbolTable<std::variant<int, float> *> constEvalSymTable;

void AST::CompileUnit::constEval(AST::Base *&root) {
    // 注意这个&，由于是引用，所以可以递归修改子树指针
    for (Base* &compileElement: compileElements) {
        constEvalHelper(compileElement);
    }
}

void AST::InitializerElement::constEval(AST::Base *&root) {
    // 可能是Expr类或InitializerList类，先做个判断
    if (std::holds_alternative<Expr *>(element)) {
        constEvalHelper(std::get<Expr *>(element));
    } else {
        constEvalHelper(std::get<InitializerList *>(element));
    }
}

void AST::InitializerList::constEval(AST::Base *&root) {
    for (InitializerElement* &element: elements) {
        constEvalHelper(element);
    }
}

void AST::ConstVariableDecl::constEval(AST::Base *&root) {
    for (ConstVariableDef* def: constVariableDefs) {
        // 对各维度求值
        for (Expr* &s: def->size) {
            constEvalHelper(s);

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
        constEvalHelper(def->initVal);

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
    for (VariableDef* def: variableDefs) {
        // 对各维度求值
        for (Expr* &s: def->size) {
            constEvalHelper(s);

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
        constEvalHelper(def->initVal);

        // 对初值进行类型转换
        // 例：float a = 1; 将(int)1转换为(float)1.0
        initializerTypeFix(def->initVal, type);
    }
}

void AST::FunctionArg::constEval(AST::Base *&root) {
    for (Expr* &s: size) {
        // 跳过第一维度的参数，例：int a[][3]，第一维为nullptr
        if (!s) {
            continue;
        }

        constEvalHelper(s);

        // 确保求值成功
        constExprCheck(s);
    }
}

void AST::Block::constEval(AST::Base *&root) {
    for (Base* &element: elements) {
        constEvalHelper(element);
    }
}

void AST::FunctionDef::constEval(AST::Base *&root) {
    // 创建该函数专属的局部符号表
    constEvalSymTable.push();

    for (FunctionArg* &argument: arguments) {
        constEvalHelper(argument);
    }
    constEvalHelper(body);

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

    for (Base* &element: elements) {
        constEvalHelper(element);
    }

    constEvalSymTable.pop();
}

void AST::IfStmt::constEval(AST::Base *&root) {
    constEvalHelper(thenStmt);
    if (elseStmt) {
        constEvalHelper(elseStmt);
    }
}

void AST::WhileStmt::constEval(AST::Base *&root) {
    constEvalHelper(body);
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
    constEvalHelper(expr);

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
    // Float > Int > Bool > Void
    // 对于每个二元运算，我们希望类型向高处转换，保证计算精度
    Typename nodeType = static_cast<Typename>(std::max(
            static_cast<int>(TypeSystem::from(L->value)),
            static_cast<int>(TypeSystem::from(R->value))
    ));

    // 按需进行类型转换
    auto Lv = L->value;
    auto Rv = R->value;
    if (TypeSystem::from(Lv) != nodeType) {
        Lv = typeFix(Lv, nodeType);
    }
    if (TypeSystem::from(Rv) != nodeType) {
        Rv = typeFix(Rv, nodeType);
    }

    // 返回转换结果、计算类型
    return {Lv, Rv, nodeType};
}

void AST::BinaryExpr::constEval(AST::Base *&root) {
    constEvalHelper(lhs);
    constEvalHelper(rhs);

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
