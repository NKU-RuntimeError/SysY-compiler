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
    std::string *strType;
    AST::Base *noneType;
    AST::Stmt *stmtType;
    AST::Expr *exprType;
    AST::ConstExpr *constExprType;
    AST::CompileUnit *compileUnitType;
    AST::CompileElement *compileElementType;
    AST::ConstVariableDecl *constVarType;
    AST::ConstVariableDef *constVarName;
    AST::VariableDecl *varType;
    AST::VariableDef *varName;
    AST::ConstVarDefList *constVarDefListType;
    AST::VarDefList *varDefListType;
    AST::ConstVarValueList *constVarValueListType;
    AST::ConstVarElement *constElementType;
    AST::VarValueList *varValueListType;
    AST::VarElement *varElementType;
    AST::ParamList *paramListType;
    AST::Array *arrayType;
    AST::FunctionDef *functionType;
    AST::FunctionArgument *functionArg;
    AST::FunctionArgList *functionListType;
    AST::Decl *declType;
    AST::Block *blockType;
    AST::BlockElement *blockElementType;
    AST::AssignStmt *assignStmtType;
    AST::ExprStmt *exprStmtType;
    AST::BlockStmt *blockStmtType;
    AST::IfStmt *ifStmtType;
    AST::WhileStmt *whileStmtType;
    AST::BreakStmt *breakStmtType;
    AST::ContiStmt *contiStmtType;
    AST::ReturnStmt *returnStmtType;
    AST::PrimaryExpr *priExprType;
    AST::UnaryExpr *uExprType;
    AST::Number *numberType;
    AST::AsmddExpr *asmddExprType;
    AST::RelationExpr *relationExprType;
    AST::LogicalExpr *logicalExprType;
    Typename typenameType;
}
// TODO: 增加更多类型，例：AST::Expr *exprType;

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
%token <strType> VALUE_INT VALUE_FLOAT
%token PLUS MINUS NOT
%token MUL DIV MOD
%token LT LE GT GE EQ NE
%token AND OR

%nterm <stmtType> stmt
%nterm <exprType> expr
%nterm <constExprType> const_expr
%nterm <compileUnitType> compile_unit_opt compile_unit
%nterm <compileElementType> compile_unit_element
%nterm <declType> decl
%nterm <constVarType> const_var_decl
%nterm <constVarName> const_var_def
%nterm <varType> var_decl
%nterm <varName> var_def
%nterm <constVarDefListType> const_var_def_list
%nterm <varDefListType>var_def_list
%nterm <constVarValueListType> const_initializer_list const_initializer_list_inner
%nterm <constElementType> const_initializer_element
%nterm <varValueListType> initializer_list initializer_list_inner
%nterm <varElementType> initializer_list_element
%nterm <paramListType> func_param_list
%nterm <arrayType> array func_arg_array lval
%nterm <functionType> func_def
%nterm <functionArg> func_arg
%nterm <functionListType> func_arg_list
%nterm <blockType> block block_inner
%nterm <blockElementType> block_element
%nterm <priExprType> primary_expr
%nterm <uExprType> unary_expr
%nterm <numberType> number
%nterm <asmddExprType> add_sub_expr mul_div_mod_expr
%nterm <relationExprType> relation_expr equal_relation_expr
%nterm <logicalExprType> logical_and_expr logical_or_expr condition
%nterm <typenameType> var_type func_type


// TODO: 增加更多非终结符类型，方便在语义动作中构建AST 例：%nterm <exprType> xxx

// 用于解决if-else的shift/reduce冲突
// 参考资料：
// https://www.gnu.org/software/bison/manual/html_node/Shift_002fReduce.html
// https://github.com/shm0214/2022NKUCS-Compilers-Lab/blob/lab7/src/parser.y

%precedence THEN
%precedence ELSE

%start compile_unit_opt

%%

compile_unit_opt
    : compile_unit {
	AST::root = $1;
	log("parser") << "set root to: " << $1 << std::endl;
    }
    | /* empty */ {
    	AST::root = Memory::make<AST::CompileUnit>();
	log("parser") << "empty compile_unit, so the root is empty" << std::endl;
    }
    ;
compile_unit
    : compile_unit compile_unit_element {
	$1->compileElements.emplace_back($2);
    	$$ = $1;
	log("parser") << "recursive merge compile_unit: " << $1 << ", " << $2 << std::endl;
    }
    | compile_unit_element {
        $$ = Memory::make<AST::CompileUnit>();
        $$->compileElements.emplace_back($1);
    	log("parser") << "lifting compile_unit_element to compile_unit: " << $1 << std::endl;
    }
    ;
// TODO: 增加下列产生式的语义动作
compile_unit_element
    : decl{
    	$$ = Memory::make<AST::CompileElement>();
	$$->type = 0;
	$$->decl = $1;
	$$->functionDef = nullptr;
	log("parser") << "compile_unit_element is decl" << std::endl;
    }
    | func_def{
    	$$ = Memory::make<AST::CompileElement>();
        $$->type = 1;
        $$->decl = nullptr;
        $$->functionDef = $1;
        log("parser") << "compile_unit_element is func_decl" << std::endl;
    }
    ;
decl
    : const_var_decl{
	$$ = $1;
	log("parser") << "decl is const_var_decl" << std::endl;
    }
    | var_decl{
     	$$ = $1;
     	log("parser") << "decl is var_decl" << std::endl;
    }
    ;
const_var_decl : CONST var_type const_var_def_list SEMICOLON{
	$$ = Memory::make<AST::ConstVariableDecl>();
        $$->constVariableDefs = $3->defs;
        $$->type = $2;
        log("parser") << "set const_var_decl type and defs" << std::endl;
    }
    ;
const_var_def_list : const_var_def_list COMMA const_var_def{
	$1->defs.emplace_back($3);
	$$ = $1;
	log("parser") << "const_var_def_list add a const_var_def" << std::endl;
    }
    | const_var_def{
    	$$ = Memory::make<AST::ConstVarDefList>();
        $$->defs.emplace_back($1);
        log("parser") << "lifting const_var_def to const_var_def_unit" << std::endl;
    }
    ;
var_type : TYPE_INT{
	$$ = Typename::INT;
	log("parser") << "find a int type" << std::endl;
    }
    | TYPE_FLOAT{
        $$ = Typename::FLOAT;
        log("parser") << "find a float type" << std::endl;
    }
    ;
const_var_def : IDENTIFIER ASSIGN const_expr{
	$$ = Memory::make<AST::ConstVariableDef>();
	$$->name = *$1;
	$$->values.emplace_back($3);
	log("parser") <<"a single identifier is set to const_var_def" << std::endl;
    }
    | array ASSIGN const_initializer_list{
    	$$ = Memory::make<AST::ConstVariableDef>();
	$$->name = $1->name;
	$$->size = $1->size;
	$$->values=$3->values;
	log("parser") << "an array is set to const_var_def" << std::endl;
    }
    ;
array : array LBRACKET const_expr RBRACKET{
	$$ = $1;
	$$->size.emplace_back($3);
	log("parser") << "increase dimension of array" << std::endl;
    }
    | IDENTIFIER LBRACKET const_expr RBRACKET{
    	$$ = Memory::make<AST::Array>();
	$$->name = *$1;
	$$->size.emplace_back($3);
	log("parser") << "one dimensional array is established" << std::endl;
    }
    ;
const_initializer_list : LBRACE const_initializer_list_inner RBRACE{
	$$ = $2;
	log("parser") << "from const_initializer_list_inner to const_initializer_list" << std::endl;
    }
    ;
const_initializer_list_inner : const_initializer_list_inner COMMA const_initializer_element{
	$$ = $1;
	$$->values.emplace_back($3);
	log("parser") << "const_initializer_list_inner add an element" << std::endl;
    }
    | const_initializer_element{
        $$ = Memory::make<AST::ConstVarValueList>();
    	$$->values.emplace_back($1);
    	log("parser") << "establish const_initializer_list_inner with a single element" << std::endl;
    }
    | /* empty */{
        $$ = Memory::make<AST::ConstVarValueList>();
	log("parser") << "establish const_initializer_list_inner with none" << std::endl;
    }
    ;
const_initializer_element : const_expr{
	$$->type = 0;
	$$->expr = $1;
	$$->list = nullptr;
	log("parser") << "const_initializer_element is a single const_expr" << std::endl;
    }
    | const_initializer_list{
    	$$->type = 1;
    	$$->expr = nullptr;
    	$$->list = $1;
    	log("parser") << "const_initializer_element is a list " << std::endl;
    }
    ;
var_decl : var_type var_def_list SEMICOLON{
	$$ = Memory::make<AST::VariableDecl>();
	$$->variableDefs = $2->defs;
	$$->type = $1;
	log("parser") << "set var_decl type and defs" << std::endl;
    }
    ;
var_def_list : var_def_list COMMA var_def{
	$1->defs.emplace_back($3);
        $$ = $1;
        log("parser") << "var_def_list add a var_def" << std::endl;
    }
    | var_def{
        $$ = Memory::make<AST::VarDefList>();
        $$->defs.emplace_back($1);
        log("parser") << "lifting const_var_def to const_var_def_unit" << std::endl;
    }
    ;
var_def : IDENTIFIER{
	$$ = Memory::make<AST::VariableDef>();
	$$->name = *$1;
	log("parser") << "give var_def a name" << std::endl;
    }
    | IDENTIFIER ASSIGN expr{
        $$ = Memory::make<AST::VariableDef>();
        $$->name = *$1;
        $$->values.emplace_back($3);
        log("parser") << "give var_def name and value" << std::endl;
    }
    | array{
    	$$ = Memory::make<AST::VariableDef>();
	$$->name = $1->name;
	$$->size = $1->size;
	log("parser") << "give var_def name of array" << std::endl;
    }
    | array ASSIGN initializer_list{
    	$$ = Memory::make<AST::VariableDef>();
	$$->name = $1->name;
	$$->size = $1->size;
	$$->values = $3->values;
	log("parser") << "give var_def name and value of array" << std::endl;
    }
    ;
initializer_list : LBRACE initializer_list_inner RBRACE{
	$$ = $2;
        log("parser") << "from initializer_list_inner to initializer_list" << std::endl;
    }
    ;
initializer_list_inner : initializer_list_inner COMMA initializer_list_element{
	$$ = $1;
	$$->values.emplace_back($3);
	log("parser") << "initializer_list_inner add an element" << std::endl;
    }
    | initializer_list_element{
        $$ = Memory::make<AST::VarValueList>();
	$$->values.emplace_back($1);
	log("parser") << "establish initializer_list_inner with a single element" << std::endl;
    }
    | /* empty */{
        $$ = Memory::make<AST::VarValueList>();
        log("parser") << "establish initializer_list_inner with none" << std::endl;
    }
    ;
initializer_list_element : expr{
	$$->type = 0;
	$$->expr = $1;
	$$->list = nullptr;
	log("parser") << "initializer_element is a single expr" << std::endl;
    }
    | initializer_list{
    	$$->type = 1;
    	$$->expr = nullptr;
    	$$->list = $1;
        log("parser") << "initializer_element is a list " << std::endl;
    }
    ;
func_def : func_type IDENTIFIER LPAREN func_arg_list RPAREN block{
	$$ = Memory::make<AST::FunctionDef>();
	$$->arguments = $4->arguments;
	$$->returnType = $1;
	$$->name = *$2;
	$$->body = $6;
	log("parser") << "a function is established" << std::endl;
    }
    ;
func_type : TYPE_VOID{
	$$ = Typename::VOID;
	log("parser") << "func_type is void" << std::endl;
    }
    | TYPE_INT{
        $$ = Typename::INT;
	log("parser") << "func_type is int" << std::endl;
    }
    | TYPE_FLOAT{
        $$ = Typename::FLOAT;
    	log("parser") << "func_type is FLOAT" << std::endl;
    }
    ;
func_arg_list : func_arg_list COMMA func_arg{
	$$ = $1;
	$$->arguments.emplace_back($3);
	log("parser") << "func_arg_list add an arg" << std::endl;
    }
    | func_arg{
    	$$ = Memory::make<AST::FunctionArgList>();
    	$$->arguments.emplace_back($1);
    	log("parser") << "establish func_arg_list with a single arg" << std::endl;
    }
    | /* empty */{
        $$ = Memory::make<AST::FunctionArgList>();
        log("parser") << "establish func_arg_list with none" << std::endl;
    }
    ;
func_arg : var_type IDENTIFIER{
	$$ = Memory::make<AST::FunctionArgument>();
	$$->type = $1;
	$$->name = *$2;
	log("parser") << "func_arg is a var" << std::endl;
    }
    | var_type func_arg_array{
    	$$ = Memory::make<AST::FunctionArgument>();
	$$->name = $2->name;
	$$->size = $2->size;
	$$->type = $1;
	log("parser") << "func_arg is an array" << std::endl;
    }
    ;
func_arg_array : func_arg_array LBRACKET const_expr RBRACKET{
	$$ = $1;
	$$->size.emplace_back($3);
	log("parser") << "increase dimension of func_arg_array" << std::endl;
    }
    | IDENTIFIER LBRACKET RBRACKET{
	$$ = Memory::make<AST::Array>();
	$$->name = *$1;
	$$->size.emplace_back(nullptr);
	log("parser") << "establish func_arg_array" << std::endl;
    }
    ;
block : LBRACE block_inner RBRACE{
	$$ = $2;
	log("parser") << "a block is established" << std::endl;
    }
    | LBRACE RBRACE{
	log("parser") << "an empty block is established" << std::endl;
    }
    ;
block_inner : block_inner block_element{
	$$ = $1;
	$$->blockElements.emplace_back($2);
	log("parser") << "block_inner add an element" << std::endl;
    }
    | block_element{
        $$ = Memory::make<AST::Block>();
        $$->blockElements.emplace_back($1);
        log("parser") << "a block_inner is established" << std::endl;
    }
    ;
block_element : decl{
	$$ = Memory::make<AST::BlockElement>();
	$$->type = 0;
	$$->decl = $1;
	$$->stmt = nullptr;
	log("parser") << "block_element is a decl" << std::endl;
    }
    | stmt{
    	$$ = Memory::make<AST::BlockElement>();
        $$->type = 1;
        $$->decl = nullptr;
        $$->stmt = $1;
        log("parser") << "block_element is a stmt" << std::endl;
    }
    ;
stmt : lval ASSIGN expr SEMICOLON{
	AST::AssignStmt *ptr = Memory::make<AST::AssignStmt>();
	ptr->lvalue = $1;
        ptr->rvalue = $3;
	$$ = ptr;
	log("parser") << "AssignStmt ->Stmt" << std::endl;
    }
    | expr SEMICOLON{
    	AST::ExprStmt *ptr = Memory::make<AST::ExprStmt>();
    	ptr->expr = $1;
    	$$ = ptr;
    	log("parser") << "ExprStmt ->Stmt" << std::endl;
    }
    | block{
    	AST::BlockStmt *ptr = Memory::make<AST::BlockStmt>();
    	ptr->block = $1;
    	$$ = ptr;
    	log("parser") << "BlockStmt ->Stmt" << std::endl;
    }
    | IF LPAREN condition RPAREN stmt %prec THEN{
    	AST::IfStmt *ptr = Memory::make<AST::IfStmt>();
    	ptr->condition = $3;
    	ptr->thenStmt = $5;
    	ptr->elseStmt = nullptr;
    	$$ = ptr;
    	log("parser") << "IfStmt ->Stmt without else" << std::endl;
    }
    | IF LPAREN condition RPAREN stmt ELSE stmt{
    	AST::IfStmt *ptr = Memory::make<AST::IfStmt>();
	ptr->condition = $3;
	ptr->thenStmt = $5;
	ptr->elseStmt = $7;
	$$ = ptr;
	log("parser") << "IfStmt ->Stmt with else" << std::endl;
    }
    | WHILE LPAREN condition RPAREN stmt{
    	AST::WhileStmt *ptr = Memory::make<AST::WhileStmt>();
	ptr->condition = $3;
	ptr->stmt = $5;
	$$ = ptr;
	log("parser") << "WhileStmt ->Stmt" << std::endl;
    }
    | BREAK SEMICOLON{
    	AST::BreakStmt *ptr = Memory::make<AST::BreakStmt>();
    	$$ = ptr;
    	log("parser") << "BreakStmt ->Stmt" << std::endl;
    }
    | CONTINUE SEMICOLON{
    	AST::ContiStmt *ptr = Memory::make<AST::ContiStmt>();
    	$$ = ptr;
    	log("parser") << "ContiStmt ->Stmt" << std::endl;
    }
    | RETURN expr SEMICOLON{
    	AST::ReturnStmt *ptr = Memory::make<AST::ReturnStmt>();
    	ptr->expr = $2;
    	$$ = ptr;
    	log("parser") << "ReturnStmt ->Stmt" << std::endl;
    }
    | RETURN SEMICOLON{
    	AST::ReturnStmt *ptr = Memory::make<AST::ReturnStmt>();
    	ptr->expr = nullptr;
    	$$ = ptr;
    	log("parser") << "ReturnStmt ->Stmt with no expr" << std::endl;
    }
    ;
expr : add_sub_expr{
	$$ = $1;
	log("parser") << "Expr is set" << std::endl;
    }
    ;
condition : logical_or_expr{
	$$ = $1;
	log("parser") << "condition is a logical_expr" << std::endl;
    }
    ;
lval : lval LBRACKET expr RBRACKET{
	$$ = $1;
	$$->size.emplace_back($3);
	log("parser") << "lval add a dimension" << std::endl;
    }
    | IDENTIFIER{
    	$$ = Memory::make<AST::Array>();
    	$$->name = *$1;
    	log("parser") << "a lval is established" << std::endl;
    }
    ;
primary_expr : LPAREN expr RPAREN{
	$$ = Memory::make<AST::PrimaryExpr>();
	$$->type = 0;
	$$->expr = $2;
	$$->lval = nullptr;
	$$->number = nullptr;
	log("parser") << "primary_expr delete paren of expr" << std::endl;
    }
    | lval{
    	$$ = Memory::make<AST::PrimaryExpr>();
    	$$->type = 1;
	$$->expr = nullptr;
	$$->lval = $1;
	$$->number = nullptr;
	log("parser") << "primary_expt get a lval" << std::endl;
    }
    | number{
    	$$ = Memory::make<AST::PrimaryExpr>();
    	$$->type = 2;
    	$$->expr = nullptr;
    	$$->lval = nullptr;
    	$$->number = $1;
    	log("parser") << "primary_expr get a number" << std::endl;
    }
    ;
number : VALUE_INT{
	$$->type = 0;
	$$->str = $1;
	log("parser") << "number is int type" << std::endl;
    }
    | VALUE_FLOAT{
    	$$->type = 1;
    	$$->str = $1;
    	log("parser") << "number is float type" << std::endl;
    }
    ;
unary_expr : primary_expr{
	$$->decType = 0;
	$$->op = 0;
	$$->pExpr = $1;
	$$->name = nullptr;
	$$->paramList = nullptr;
	log("parser") << "unary_expr is from primary_expr" << std::endl;
    }
    | IDENTIFIER LPAREN func_param_list RPAREN{
    	$$->decType = 1;
	$$->op = 0;
	$$->pExpr = nullptr;
	$$->name = $1;
	$$->paramList = $3;
	log("parser") << "unary_expr is from func_param_list" << std::endl;
    }
    | PLUS unary_expr{
    	$$->op = 0;
    	log("parser") << "unary_expr is PLUS" << std::endl;
    }
    | MINUS unary_expr{
    	$$->op = 1;
	log("parser") << "unary_expr is MINUS" << std::endl;
    }
    | NOT unary_expr{
    	$$->op = 2;
	log("parser") << "unary_expr is NOT" << std::endl;
    }
    ;
func_param_list : func_param_list COMMA expr{
	$$ = $1;
	$$->exprs.emplace_back($3);
	log("parser") << "func_param_list add a expr" << std::endl;
    }
    | expr{
    	$$ = Memory::make<AST::ParamList>();
    	$$->exprs.emplace_back($1);
    	log("parser") << "a func_param_list is established" << std::endl;
    }
    | /* empty */{
    	$$ = Memory::make<AST::ParamList>();
    	log("parser") << "an empty func_param_list is established" << std::endl;
    }
    ;
mul_div_mod_expr : unary_expr{
	$$ = Memory::make<AST::AsmddExpr>();
	$$->lexpr = $1;
	$$->rexpr = nullptr;
	log("parser") << "mul_div_mod_expr is a unary_expr" << std::endl;
    }
    | mul_div_mod_expr MUL unary_expr{
    	$$->op = 2;
    	$$->lexpr = $1;
    	$$->rexpr = $3;
    	log("parser") << "mul_div_mod_expr do MUL" << std::endl;
    }
    | mul_div_mod_expr DIV unary_expr{
    	$$->op = 3;
	$$->lexpr = $1;
	$$->rexpr = $3;
	log("parser") << "mul_div_mod_expr do DIV" << std::endl;
    }
    | mul_div_mod_expr MOD unary_expr{
    	$$->op = 4;
	$$->lexpr = $1;
	$$->rexpr = $3;
	log("parser") << "mul_div_mod_expr do MOD" << std::endl;
    }
    ;
add_sub_expr : mul_div_mod_expr{
	$$ = Memory::make<AST::AsmddExpr>();
	$$->lexpr = $1;
	$$->rexpr = nullptr;
	log("parser") << "add_sub_expr is a mul_div_mod_expr" << std::endl;
    }
    | add_sub_expr PLUS mul_div_mod_expr{
    	$$->op = 0;
	$$->lexpr = $1;
	$$->rexpr = $3;
	log("parser") << "add_sub_expr do PLUS" << std::endl;
    }
    | add_sub_expr MINUS mul_div_mod_expr{
	$$->op = 1;
    	$$->lexpr = $1;
    	$$->rexpr = $3;
    	log("parser") << "add_sub_expr do MINUS" << std::endl;
    }
    ;
relation_expr : add_sub_expr{
	$$ = Memory::make<AST::RelationExpr>();
	$$->lexpr = $1;
	$$->rexpr = nullptr;
	log("parser") << "relation_expr is an add_sub_expr" << std::endl;
    }
    | relation_expr LT add_sub_expr{
	$$->op = 2;
        $$->lexpr = $1;
        $$->rexpr = $3;
        log("parser") << "relation_expr do LT" << std::endl;
    }
    | relation_expr GT add_sub_expr{
    	$$->op = 3;
	$$->lexpr = $1;
	$$->rexpr = $3;
	log("parser") << "relation_expr do GT" << std::endl;
    }
    | relation_expr LE add_sub_expr{
    	$$->op = 4;
    	$$->lexpr = $1;
    	$$->rexpr = $3;
    	log("parser") << "relation_expr do LE" << std::endl;
    }
    | relation_expr GE add_sub_expr{
	$$->op = 5;
        $$->lexpr = $1;
        $$->rexpr = $3;
        log("parser") << "relation_expr do GE" << std::endl;
    }
    ;
equal_relation_expr : relation_expr{
	$$ = Memory::make<AST::RelationExpr>();
	$$->lexpr = $1;
	$$->rexpr = nullptr;
	log("parser") << "equal_relation_expr is a relation_expr" << std::endl;
    }
    | equal_relation_expr EQ relation_expr{
    	$$->op = 0;
	$$->lexpr = $1;
	$$->rexpr = $3;
	log("parser") << "equal_relation_expr do EQ" << std::endl;
    }
    | equal_relation_expr NE relation_expr{
    	$$->op = 1;
    	$$->lexpr = $1;
    	$$->rexpr = $3;
    	log("parser") << "equal_relation_expr do NE" << std::endl;
    }
    ;
logical_and_expr : equal_relation_expr{
	$$ = Memory::make<AST::LogicalExpr>();
	$$->lexpr = $1;
	$$->rexpr = nullptr;
	log("parser") << "logical_and_expr is an equal_relation_expr" << std::endl;
    }
    | logical_and_expr AND equal_relation_expr{
    	$$->op = 0;
	$$->lexpr = $1;
	$$->rexpr = $3;
	log("parser") << "logical_and_expr do AND" << std::endl;
    }
    ;
logical_or_expr : logical_and_expr{
	$$ = Memory::make<AST::LogicalExpr>();
	$$->lexpr = $1;
	$$->rexpr = nullptr;
	log("parser") << "logical_or_expr is a logical_and_expr" << std::endl;
    }
    | logical_or_expr OR logical_and_expr{
    	$$->op = 1;
	$$->lexpr = $1;
	$$->rexpr = $3;
	log("parser") << "logical_or_expr do AND" << std::endl;
    }
    ;
const_expr : add_sub_expr{
	$$ = $1;
	log("parser") << "const_expr is an add_sub_expr" << std::endl;
    }
    ;

%%

void yyerror(const char* s) {
    err("parser") << s << std::endl;
    std::exit(1);
}
