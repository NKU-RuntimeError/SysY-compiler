#include <utility>
#include <llvm/Support/JSON.h>
#include "magic_enum.h"
#include "AST.h"

llvm::json::Value AST::CompileUnit::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "CompileUnit";
    llvm::json::Array jsonCompileElements;
    for (auto &compileElement: compileElements) {
        jsonCompileElements.emplace_back(std::move(compileElement->toJSON()));
    }
    obj["compileElements"] = std::move(jsonCompileElements);
    return obj;
}

llvm::json::Value AST::InitializerElement::toJSON() {
    if (std::holds_alternative<Expr *>(element)) {
        return std::get<Expr *>(element)->toJSON();
    } else {
        return std::get<InitializerList *>(element)->toJSON();
    }
}

llvm::json::Value AST::InitializerList::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "InitializerList";
    llvm::json::Array jsonElements;
    for (auto &element: elements) {
        jsonElements.emplace_back(std::move(element->toJSON()));
    }
    obj["elements"] = std::move(jsonElements);
    return obj;
}

llvm::json::Value AST::ConstVariableDef::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "ConstVariableDef";
    obj["name"] = name;
    llvm::json::Array jsonSize;
    for (auto &s: size) {
        jsonSize.emplace_back(std::move(s->toJSON()));
    }
    obj["size"] = std::move(jsonSize);
    obj["initVal"] = std::move(initVal->toJSON());
    return obj;
}

llvm::json::Value AST::ConstVariableDecl::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "ConstVariableDecl";
    obj["type"] = std::string(magic_enum::enum_name(type));
    llvm::json::Array jsonConstVariableDefs;
    for (auto &constVariableDef: constVariableDefs) {
        jsonConstVariableDefs.emplace_back(std::move(constVariableDef->toJSON()));
    }
    obj["constVariableDefs"] = std::move(jsonConstVariableDefs);
    return obj;
}

llvm::json::Value AST::VariableDef::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "VariableDef";
    obj["name"] = name;
    llvm::json::Array jsonSize;
    for (auto &s: size) {
        jsonSize.emplace_back(std::move(s->toJSON()));
    }
    obj["size"] = std::move(jsonSize);
    if (initVal) {
        obj["initVal"] = std::move(initVal->toJSON());
    }
    return obj;
}

llvm::json::Value AST::VariableDecl::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "VariableDecl";
    obj["type"] = std::string(magic_enum::enum_name(type));
    llvm::json::Array jsonVariableDefs;
    for (auto &variableDef: variableDefs) {
        jsonVariableDefs.emplace_back(std::move(variableDef->toJSON()));
    }
    obj["variableDefs"] = std::move(jsonVariableDefs);
    return obj;
}

llvm::json::Value AST::FunctionArg::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "FunctionArg";
    obj["type"] = std::string(magic_enum::enum_name(type));
    obj["name"] = name;
    llvm::json::Array jsonSize;
    for (auto &s: size) {
        if (!s) {
            jsonSize.emplace_back("null");
        } else {
            jsonSize.emplace_back(std::move(s->toJSON()));
        }
    }
    obj["size"] = std::move(jsonSize);
    return obj;
}


llvm::json::Value AST::Block::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "Block";
    llvm::json::Array jsonElements;
    for (auto &element: elements) {
        jsonElements.emplace_back(std::move(element->toJSON()));
    }
    obj["elements"] = std::move(jsonElements);
    return obj;
}

llvm::json::Value AST::FunctionDef::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "FunctionDef";
    obj["returnType"] = std::string(magic_enum::enum_name(returnType));
    obj["name"] = name;
    llvm::json::Array jsonArguments;
    for (auto &argument: arguments) {
        jsonArguments.emplace_back(std::move(argument->toJSON()));
    }
    obj["arguments"] = std::move(jsonArguments);
    obj["body"] = std::move(body->toJSON());
    return obj;
}

llvm::json::Value AST::LValue::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "LValue";
    obj["name"] = name;
    llvm::json::Array jsonSize;
    for (auto &s: size) {
        jsonSize.emplace_back(std::move(s->toJSON()));
    }
    obj["size"] = std::move(jsonSize);
    return obj;
}

llvm::json::Value AST::AssignStmt::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "AssignStmt";
    obj["lValue"] = std::move(lValue->toJSON());
    obj["rValue"] = std::move(rValue->toJSON());
    return obj;
}

llvm::json::Value AST::ExprStmt::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "ExprStmt";
    obj["expr"] = std::move(expr->toJSON());
    return obj;
}

llvm::json::Value AST::NullStmt::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "NullStmt";
    return obj;
}

llvm::json::Value AST::BlockStmt::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "BlockStmt";
    llvm::json::Array jsonElements;
    for (auto &element: elements) {
        jsonElements.emplace_back(std::move(element->toJSON()));
    }
    obj["elements"] = std::move(jsonElements);
    return obj;
}

llvm::json::Value AST::IfStmt::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "IfStmt";
    obj["condition"] = std::move(condition->toJSON());
    obj["thenStmt"] = std::move(thenStmt->toJSON());
    if (elseStmt) {
        obj["elseStmt"] = std::move(elseStmt->toJSON());
    }
    return obj;
}

llvm::json::Value AST::WhileStmt::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "WhileStmt";
    obj["condition"] = std::move(condition->toJSON());
    obj["body"] = std::move(body->toJSON());
    return obj;
}

llvm::json::Value AST::BreakStmt::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "BreakStmt";
    return obj;
}

llvm::json::Value AST::ContinueStmt::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "ContinueStmt";
    return obj;
}

llvm::json::Value AST::ReturnStmt::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "ReturnStmt";
    if (expr) {
        obj["expr"] = std::move(expr->toJSON());
    }
    return obj;
}

llvm::json::Value AST::UnaryExpr::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "UnaryExpr";
    obj["op"] = std::string(magic_enum::enum_name(op));
    obj["expr"] = std::move(expr->toJSON());
    return obj;
}

llvm::json::Value AST::FunctionCallExpr::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "FunctionCallExpr";
    obj["name"] = name;
    llvm::json::Array jsonParams;
    for (auto &param: params) {
        jsonParams.emplace_back(std::move(param->toJSON()));
    }
    obj["params"] = std::move(jsonParams);
    return obj;
}

llvm::json::Value AST::BinaryExpr::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "BinaryExpr";
    obj["op"] = std::string(magic_enum::enum_name(op));
    obj["lhs"] = std::move(lhs->toJSON());
    obj["rhs"] = std::move(rhs->toJSON());
    return obj;
}

llvm::json::Value AST::NumberExpr::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "NumberExpr";
    obj["type"] = std::string(magic_enum::enum_name(type));
    if (type == Typename::INT) {
        obj["value"] = std::get<int>(value);
    } else {
        obj["value"] = std::get<float>(value);
    }
    return obj;
}

llvm::json::Value AST::VariableExpr::toJSON() {
    llvm::json::Object obj;
    obj["NODE_TYPE"] = "VariableExpr";
    obj["name"] = name;
    llvm::json::Array jsonSize;
    for (auto &s: size) {
        jsonSize.emplace_back(std::move(s->toJSON()));
    }
    obj["size"] = std::move(jsonSize);
    return obj;
}
