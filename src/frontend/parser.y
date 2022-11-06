%{
#include <iostream>
#include <string>
#include "log.h"
#include "lexer.h"
int yylex();
extern int yyparse();
void yyerror(const char* s);
%}

%code requires {
#include "type.h"
#include "mem.h"
#include "AST.h"
}

// 在变量声明和函数声明，由于前序均为 TYPENAME IDENTIFIER
// 因此在解析到TYPENAME时，无法确定reduce到哪一个产生式，因此会产生reduce/reduce冲突
// 本质原因是因为bison是LR(1)的解析器，只有一个lookahaed的token
// bison手册给出的解决方案是开启glr模式，在这种模式下，bison会对每条可能的分支都进行遍历
// 正确的分支继续向下执行，错误的分支停止执行
// 由于在看到括号后，即可确定是变量声明还是函数声明，因此仍为线性时间复杂度。
// 参考资料：
// https://www.gnu.org/software/bison/manual/html_node/Simple-GLR-Parsers.html
// https://www.gnu.org/software/bison/manual/html_node/GLR-Parsers.html

%glr-parser
%expect-rr 2

%union {
    AST::Base *baseType;
    AST::Stmt *stmtType;
    AST::Expr *exprType;
    AST::Decl *declType;
    AST::CompileUnit *compileUnitType;
    AST::InitializerElement *initializerElementType;
    AST::InitializerList *initializerListType;
    AST::Array *arrayType;
    AST::ConstVariableDef *constVariableDefType;
    AST::ConstVariableDefList *constVariableDefListType;
    AST::ConstVariableDecl *constVariableDeclType;
    AST::VariableDef *variableDefType;
    AST::VariableDefList *variableDefListType;
    AST::VariableDecl *variableDeclType;
    AST::FunctionArg *functionArgType;
    AST::FunctionArgList *functionArgListType;
    AST::Block *blockType;
    AST::FunctionDef *functionDefType;
    AST::LValue *lValueType;
    AST::AssignStmt *assignStmtType;
    AST::ExprStmt *exprStmtType;
    AST::BlockStmt *blockStmtType;
    AST::IfStmt *ifStmtType;
    AST::WhileStmt *whileStmtType;
    AST::BreakStmt *breakStmtType;
    AST::ContinueStmt *continueStmtType;
    AST::ReturnStmt *returnStmtType;
    AST::UnaryExpr *unaryExprType;
    AST::FunctionParamList *functionParamListType;
    AST::FunctionCallExpr *functionCallExprType;
    AST::BinaryExpr *binaryExprType;
    AST::NumberExpr *numberExprType;
    AST::NullStmt *nullStmtType;
    AST::VariableExpr *variableExprType;
    Typename typenameType;
    Operator operatorType;
    std::string *strType;
    int intType;
    float floatType;
}

%token CONST
%token COMMA
%token SEMICOLON
%token TYPE_INT TYPE_FLOAT TYPE_VOID
%token <strType> IDENTIFIER
%token LBRACE RBRACE
%token LBRACKET RBRACKET
%token LPAREN RPAREN
%token ASSIGN
%token IF ELSE
%token WHILE BREAK CONTINUE
%token RETURN
%token <intType> VALUE_INT
%token <floatType> VALUE_FLOAT
%token PLUS MINUS NOT
%token MUL DIV MOD
%token LT LE GT GE EQ NE
%token AND OR

// 用于解决if-else的shift/reduce冲突
// 参考资料：
// https://www.gnu.org/software/bison/manual/html_node/Shift_002fReduce.html
// https://github.com/shm0214/2022NKUCS-Compilers-Lab/blob/lab7/src/parser.y

%precedence THEN
%precedence ELSE

%start compile_unit_opt

%nterm <compileUnitType> compile_unit_opt compile_unit
%nterm <baseType> compile_unit_element
%nterm <declType> decl
%nterm <constVariableDeclType> const_var_decl
%nterm <constVariableDefListType> const_var_def_list
%nterm <typenameType> var_type
%nterm <constVariableDefType> const_var_def
%nterm <arrayType> identifier_or_array
%nterm <initializerListType> const_initializer_list const_initializer_list_inner
%nterm <initializerElementType> const_initializer_element
%nterm <variableDeclType> var_decl
%nterm <variableDefListType> var_def_list
%nterm <variableDefType> var_def
%nterm <initializerListType> initializer_list initializer_list_inner
%nterm <initializerElementType> initializer_element
%nterm <functionDefType> func_def
%nterm <typenameType> func_type
%nterm <functionArgListType> func_arg_list
%nterm <functionArgType> func_arg func_arg_identifier_or_array func_arg_array
%nterm <blockType> block block_inner
%nterm <baseType> block_element
%nterm <stmtType> stmt
%nterm <exprType> expr
%nterm <exprType> condition
%nterm <lValueType> lval
%nterm <exprType> primary_expr
%nterm <exprType> number
%nterm <exprType> unary_expr
%nterm <functionParamListType> func_param_list
%nterm <exprType> mul_div_mod_expr add_sub_expr relation_expr equal_relation_expr logical_and_expr logical_or_expr
%nterm <exprType> const_expr

// 在AST中，为了更清晰的表达AST节点的构造过程，不使用构造函数进行初始化，而是在创建节点后，手动设置各个属性
// 如果使用构造函数，则容易导致代码难以阅读

%%

compile_unit_opt
    : compile_unit {
	AST::root = $1;
    }
    | /* empty */ {
    	AST::root = Memory::make<AST::CompileUnit>();
    }
    ;

compile_unit
    : compile_unit compile_unit_element {
	$1->compileElements.emplace_back($2);
    	$$ = $1;
    }
    | compile_unit_element {
        $$ = Memory::make<AST::CompileUnit>();
        $$->compileElements.emplace_back($1);
    }
    ;

compile_unit_element
    : decl {
        $$ = $1;
    }
    | func_def {
        $$ = $1;
    }
    ;

decl
    : const_var_decl {
        $$ = $1;
    }
    | var_decl {
        $$ = $1;
    }
    ;

const_var_decl
    : CONST var_type const_var_def_list SEMICOLON {
        $$ = Memory::make<AST::ConstVariableDecl>();
	$$->type = $2;
	$$->constVariableDefs = $3->constVariableDefs;
    }
    ;

const_var_def_list
    : const_var_def_list COMMA const_var_def {
        $1->constVariableDefs.emplace_back($3);
	$$ = $1;
    }
    | const_var_def {
        $$ = Memory::make<AST::ConstVariableDefList>();
        $$->constVariableDefs.emplace_back($1);
    }
    ;

var_type
    : TYPE_INT {
        $$ = Typename::INT;
    }
    | TYPE_FLOAT {
        $$ = Typename::FLOAT;
    }
    ;

const_var_def
    : identifier_or_array ASSIGN const_initializer_element {
        $$ = Memory::make<AST::ConstVariableDef>();
        $$->name = $1->name;
	$$->size = $1->size;
	$$->initVal = $3;
    }
    ;

identifier_or_array
    : identifier_or_array LBRACKET const_expr RBRACKET {
        $1->size.emplace_back($3);
	$$ = $1;
    }
    | IDENTIFIER {
        $$ = Memory::make<AST::Array>();
	$$->name = *$1;
    }
    ;

const_initializer_list
    : LBRACE const_initializer_list_inner RBRACE {
        $$ = $2;
    }
    ;

const_initializer_list_inner
    : const_initializer_list_inner COMMA const_initializer_element {
        $1->elements.emplace_back($3);
	$$ = $1;
    }
    | const_initializer_element {
        $$ = Memory::make<AST::InitializerList>();
	$$->elements.emplace_back($1);
    }
    | /* empty */ {
        $$ = Memory::make<AST::InitializerList>();
    }
    ;

const_initializer_element
    : const_expr {
        $$ = Memory::make<AST::InitializerElement>();
	$$->element = $1;
    }
    | const_initializer_list {
	$$ = Memory::make<AST::InitializerElement>();
	$$->element = $1;
    }
    ;

var_decl
    : var_type var_def_list SEMICOLON {
        $$ = Memory::make<AST::VariableDecl>();
	$$->type = $1;
	$$->variableDefs = $2->variableDefs;
    }
    ;

var_def_list
    : var_def_list COMMA var_def {
	$1->variableDefs.emplace_back($3);
	$$ = $1;
    }
    | var_def {
        $$ = Memory::make<AST::VariableDefList>();
        $$->variableDefs.emplace_back($1);
    }
    ;

var_def
    : identifier_or_array {
        $$ = Memory::make<AST::VariableDef>();
	$$->name = $1->name;
	$$->size = $1->size;
	$$->initVal = nullptr;
    }
    | identifier_or_array ASSIGN initializer_element {
        $$ = Memory::make<AST::VariableDef>();
	$$->name = $1->name;
	$$->size = $1->size;
	$$->initVal = $3;
    }
    ;

initializer_list
    : LBRACE initializer_list_inner RBRACE {
        $$ = $2;
    }
    ;

initializer_list_inner
    : initializer_list_inner COMMA initializer_element {
        $1->elements.emplace_back($3);
	$$ = $1;
    }
    | initializer_element {
        $$ = Memory::make<AST::InitializerList>();
	$$->elements.emplace_back($1);
    }
    | /* empty */ {
        $$ = Memory::make<AST::InitializerList>();
    }
    ;

initializer_element
    : expr {
        $$ = Memory::make<AST::InitializerElement>();
	$$->element = $1;
    }
    | initializer_list {
    	$$ = Memory::make<AST::InitializerElement>();
	$$->element = $1;
    }
    ;

func_def
    : func_type IDENTIFIER LPAREN func_arg_list RPAREN block {
        $$ = Memory::make<AST::FunctionDef>();
	$$->returnType = $1;
	$$->name = *$2;
	$$->arguments = $4->arguments;
	$$->body = $6;
    }
    ;

func_type
    : TYPE_VOID {
        $$ = Typename::VOID;
    }
    | TYPE_INT {
	$$ = Typename::INT;
    }
    | TYPE_FLOAT {
    	$$ = Typename::FLOAT;
    }
    ;

func_arg_list
    : func_arg_list COMMA func_arg {
        $1->arguments.emplace_back($3);
	$$ = $1;
    }
    | func_arg {
        $$ = Memory::make<AST::FunctionArgList>();
	$$->arguments.emplace_back($1);
    }
    | /* empty */ {
        $$ = Memory::make<AST::FunctionArgList>();
    }
    ;

func_arg
    : var_type func_arg_identifier_or_array {
        $2->type = $1;
	$$ = $2;
    }
    ;

func_arg_identifier_or_array
    : IDENTIFIER {
        $$ = Memory::make<AST::FunctionArg>();
	$$->name = *$1;
    }
    | func_arg_array {
        $$ = $1;
    }
    ;

func_arg_array
    : func_arg_array LBRACKET const_expr RBRACKET {
        $1->size.emplace_back($3);
	$$ = $1;
    }
    | IDENTIFIER LBRACKET RBRACKET {
        $$ = Memory::make<AST::FunctionArg>();
	$$->name = *$1;
	$$->size.emplace_back(nullptr);
    }
    ;

block
    : LBRACE block_inner RBRACE {
        $$ = $2;
    }
    | LBRACE RBRACE {
        $$ = Memory::make<AST::Block>();
    }
    ;

block_inner
    : block_inner block_element {
        $1->elements.emplace_back($2);
	$$ = $1;
    }
    | block_element {
        $$ = Memory::make<AST::Block>();
	$$->elements.emplace_back($1);
    }
    ;

block_element
    : decl {
        $$ = $1;
    }
    | stmt {
        $$ = $1;
    }
    ;

stmt
    : lval ASSIGN expr SEMICOLON {
        auto ptr = Memory::make<AST::AssignStmt>();
	ptr->lValue = $1;
	ptr->rValue = $3;
	$$ = ptr;
    }
    | expr SEMICOLON {
        auto ptr = Memory::make<AST::ExprStmt>();
	ptr->expr = $1;
	$$ = ptr;
    }
    | SEMICOLON {
        $$ = Memory::make<AST::NullStmt>();
    }
    | block {
    	auto ptr = Memory::make<AST::BlockStmt>();
	ptr->elements = $1->elements;
	$$ = ptr;
    }
    | IF LPAREN condition RPAREN stmt %prec THEN {
        auto ptr = Memory::make<AST::IfStmt>();
	ptr->condition = $3;
	ptr->thenStmt = $5;
	ptr->elseStmt = nullptr;
	$$ = ptr;
    }
    | IF LPAREN condition RPAREN stmt ELSE stmt {
        auto ptr = Memory::make<AST::IfStmt>();
	ptr->condition = $3;
	ptr->thenStmt = $5;
	ptr->elseStmt = $7;
	$$ = ptr;
    }
    | WHILE LPAREN condition RPAREN stmt {
        auto ptr = Memory::make<AST::WhileStmt>();
	ptr->condition = $3;
	ptr->body = $5;
	$$ = ptr;
    }
    | BREAK SEMICOLON {
        $$ = Memory::make<AST::BreakStmt>();
    }
    | CONTINUE SEMICOLON {
        $$ = Memory::make<AST::ContinueStmt>();
    }
    | RETURN expr SEMICOLON {
        auto ptr = Memory::make<AST::ReturnStmt>();
	ptr->expr = $2;
	$$ = ptr;
    }
    | RETURN SEMICOLON {
        auto ptr = Memory::make<AST::ReturnStmt>();
	ptr->expr = nullptr;
	$$ = ptr;
    }
    ;

expr
    : add_sub_expr {
        $$ = $1;
    }
    ;

condition
    : logical_or_expr {
        $$ = $1;
    }
    ;

lval
    : lval LBRACKET expr RBRACKET {
   	$1->size.emplace_back($3);
	$$ = $1;
    }
    | IDENTIFIER {
        $$ = Memory::make<AST::LValue>();
	$$->name = *$1;
    }
    ;

primary_expr
    : LPAREN expr RPAREN {
        $$ = $2;
    }
    | lval {
        auto ptr = Memory::make<AST::VariableExpr>();
	ptr->name = $1->name;
	ptr->size = $1->size;
	$$ = ptr;
    }
    | number {
        $$ = $1;
    }
    ;

number
    : VALUE_INT {
        auto ptr = Memory::make<AST::NumberExpr>();
	ptr->value = $1;
	$$ = ptr;
    }
    | VALUE_FLOAT {
        auto ptr = Memory::make<AST::NumberExpr>();
	ptr->value = $1;
	$$ = ptr;
    }
    ;

unary_expr
    : primary_expr {
        $$ = $1;
    }
    | IDENTIFIER LPAREN func_param_list RPAREN {
        auto ptr = Memory::make<AST::FunctionCallExpr>();
	ptr->name = *$1;
	ptr->params = $3->params;
	$$ = ptr;
    }
    | PLUS unary_expr {
        auto ptr = Memory::make<AST::UnaryExpr>();
	ptr->op = Operator::ADD;
	ptr->expr = $2;
	$$ = ptr;
    }
    | MINUS unary_expr {
        auto ptr = Memory::make<AST::UnaryExpr>();
	ptr->op = Operator::SUB;
	ptr->expr = $2;
	$$ = ptr;
    }
    | NOT unary_expr {
        auto ptr = Memory::make<AST::UnaryExpr>();
	ptr->op = Operator::NOT;
	ptr->expr = $2;
	$$ = ptr;
    }
    ;

func_param_list
    : func_param_list COMMA expr {
	$1->params.emplace_back($3);
	$$ = $1;
    }
    | expr {
   	$$ = Memory::make<AST::FunctionParamList>();
	$$->params.emplace_back($1);
    }
    | /* empty */ {
        $$ = Memory::make<AST::FunctionParamList>();
    }
    ;

mul_div_mod_expr
    : unary_expr {
        $$ = $1;
    }
    | mul_div_mod_expr MUL unary_expr {
        auto ptr = Memory::make<AST::BinaryExpr>();
	ptr->op = Operator::MUL;
	ptr->lhs = $1;
	ptr->rhs = $3;
	$$ = ptr;
    }
    | mul_div_mod_expr DIV unary_expr {
        auto ptr = Memory::make<AST::BinaryExpr>();
	ptr->op = Operator::DIV;
	ptr->lhs = $1;
	ptr->rhs = $3;
	$$ = ptr;
    }
    | mul_div_mod_expr MOD unary_expr {
        auto ptr = Memory::make<AST::BinaryExpr>();
	ptr->op = Operator::MOD;
	ptr->lhs = $1;
	ptr->rhs = $3;
	$$ = ptr;
    }
    ;

add_sub_expr
    : mul_div_mod_expr {
        $$ = $1;
    }
    | add_sub_expr PLUS mul_div_mod_expr {
        auto ptr = Memory::make<AST::BinaryExpr>();
	ptr->op = Operator::ADD;
	ptr->lhs = $1;
	ptr->rhs = $3;
	$$ = ptr;
    }
    | add_sub_expr MINUS mul_div_mod_expr {
        auto ptr = Memory::make<AST::BinaryExpr>();
	ptr->op = Operator::SUB;
	ptr->lhs = $1;
	ptr->rhs = $3;
	$$ = ptr;
    }
    ;

relation_expr
    : add_sub_expr {
        $$ = $1;
    }
    | relation_expr LT add_sub_expr {
        auto ptr = Memory::make<AST::BinaryExpr>();
	ptr->op = Operator::LT;
	ptr->lhs = $1;
	ptr->rhs = $3;
	$$ = ptr;
    }
    | relation_expr GT add_sub_expr{
        auto ptr = Memory::make<AST::BinaryExpr>();
	ptr->op = Operator::GT;
	ptr->lhs = $1;
	ptr->rhs = $3;
	$$ = ptr;
    }
    | relation_expr LE add_sub_expr {
        auto ptr = Memory::make<AST::BinaryExpr>();
	ptr->op = Operator::LE;
	ptr->lhs = $1;
	ptr->rhs = $3;
	$$ = ptr;
    }
    | relation_expr GE add_sub_expr {
        auto ptr = Memory::make<AST::BinaryExpr>();
	ptr->op = Operator::GE;
	ptr->lhs = $1;
	ptr->rhs = $3;
	$$ = ptr;
    }
    ;

equal_relation_expr
    : relation_expr {
        $$ = $1;
    }
    | equal_relation_expr EQ relation_expr {
        auto ptr = Memory::make<AST::BinaryExpr>();
	ptr->op = Operator::EQ;
	ptr->lhs = $1;
	ptr->rhs = $3;
	$$ = ptr;
    }
    | equal_relation_expr NE relation_expr {
        auto ptr = Memory::make<AST::BinaryExpr>();
	ptr->op = Operator::NE;
	ptr->lhs = $1;
	ptr->rhs = $3;
	$$ = ptr;
    }
    ;

logical_and_expr
    : equal_relation_expr {
        $$ = $1;
    }
    | logical_and_expr AND equal_relation_expr {
        auto ptr = Memory::make<AST::BinaryExpr>();
	ptr->op = Operator::AND;
	ptr->lhs = $1;
	ptr->rhs = $3;
	$$ = ptr;
    }
    ;

logical_or_expr
    : logical_and_expr {
        $$ = $1;
    }
    | logical_or_expr OR logical_and_expr {
        auto ptr = Memory::make<AST::BinaryExpr>();
	ptr->op = Operator::OR;
	ptr->lhs = $1;
	ptr->rhs = $3;
	$$ = ptr;
    }
    ;

const_expr
    : add_sub_expr {
        $$ = $1;
    }
    ;

%%

void yyerror(const char* s) {
    err("parser") << s << std::endl;
    std::exit(1);
}
