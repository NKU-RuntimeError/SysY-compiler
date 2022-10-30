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
    AST::FunctionCallExpr *functionCallExprType;
    AST::BinaryExpr *binaryExprType;
    AST::NumberExpr *numberExprType;
    Typename typenameType;
    Operator operatorType;
    std::string *strType;
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



// TODO: 增加更多非终结符类型，方便在语义动作中构建AST 例：%nterm <exprType> xxx

// 用于解决if-else的shift/reduce冲突
// 参考资料：
// https://www.gnu.org/software/bison/manual/html_node/Shift_002fReduce.html
// https://github.com/shm0214/2022NKUCS-Compilers-Lab/blob/lab7/src/parser.y

%precedence THEN
%precedence ELSE

%start compile_unit_opt

%nterm <compileUnitType> compile_unit_opt compile_unit compile_unit_element

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
compile_unit_element : decl
                     | func_def
                     ;
decl : const_var_decl
     | var_decl
     ;
const_var_decl : CONST var_type const_var_def_list SEMICOLON
               ;
const_var_def_list : const_var_def_list COMMA const_var_def
                   | const_var_def
                   ;
var_type : TYPE_INT
         | TYPE_FLOAT
         ;
const_var_def : identifier_or_array ASSIGN const_initializer_element
              ;
identifier_or_array : identifier_or_array LBRACKET const_expr RBRACKET
                    | IDENTIFIER
                    ;
const_initializer_list : LBRACE const_initializer_list_inner RBRACE
                       ;
const_initializer_list_inner : const_initializer_list_inner COMMA const_initializer_element
                             | const_initializer_element
                             | /* empty */
                             ;
const_initializer_element : const_expr
                          | const_initializer_list
                          ;
var_decl : var_type var_def_list SEMICOLON
         ;
var_def_list : var_def_list COMMA var_def
             | var_def
             ;
var_def : identifier_or_array
        | identifier_or_array ASSIGN initializer_element
        ;
initializer_list : LBRACE initializer_list_inner RBRACE
                 ;
initializer_list_inner : initializer_list_inner COMMA initializer_element
                       | initializer_element
                       | /* empty */
                       ;
initializer_element : expr
                         | initializer_list
                         ;
func_def : func_type IDENTIFIER LPAREN func_arg_list RPAREN block
         ;
func_type : TYPE_VOID
	  | TYPE_INT
	  | TYPE_FLOAT
          ;
func_arg_list : func_arg_list COMMA func_arg
              | func_arg
              | /* empty */
              ;
func_arg : var_type func_arg_identifier_or_array
         ;
func_arg_identifier_or_array : IDENTIFIER
                             | func_arg_array
                             ;
func_arg_array : func_arg_array LBRACKET const_expr RBRACKET
               | IDENTIFIER LBRACKET RBRACKET
               ;
block : LBRACE block_inner RBRACE
      | LBRACE RBRACE
      ;
block_inner : block_inner block_element
            | block_element
            ;
block_element : decl
              | stmt
              ;
stmt : lval ASSIGN expr SEMICOLON
     | expr SEMICOLON
     | SEMICOLON
     | block
     | IF LPAREN condition RPAREN stmt %prec THEN
     | IF LPAREN condition RPAREN stmt ELSE stmt
     | WHILE LPAREN condition RPAREN stmt
     | BREAK SEMICOLON
     | CONTINUE SEMICOLON
     | RETURN expr SEMICOLON
     | RETURN SEMICOLON
     ;
expr : add_sub_expr
     ;
condition : logical_or_expr
          ;
lval : lval LBRACKET expr RBRACKET
     | IDENTIFIER
     ;
primary_expr : LPAREN expr RPAREN
             | lval
             | number
             ;
number : VALUE_INT
       | VALUE_FLOAT
       ;
unary_expr : primary_expr
           | IDENTIFIER LPAREN func_param_list RPAREN
           | PLUS unary_expr
           | MINUS unary_expr
           | NOT unary_expr
           ;
func_param_list : func_param_list COMMA expr
                | expr
                | /* empty */
                ;
mul_div_mod_expr : unary_expr
                 | mul_div_mod_expr MUL unary_expr
                 | mul_div_mod_expr DIV unary_expr
                 | mul_div_mod_expr MOD unary_expr
                 ;
add_sub_expr : mul_div_mod_expr
             | add_sub_expr PLUS mul_div_mod_expr
             | add_sub_expr MINUS mul_div_mod_expr
             ;
relation_expr : add_sub_expr
              | relation_expr LT add_sub_expr
              | relation_expr GT add_sub_expr
              | relation_expr LE add_sub_expr
              | relation_expr GE add_sub_expr
              ;
equal_relation_expr : relation_expr
                    | equal_relation_expr EQ relation_expr
                    | equal_relation_expr NE relation_expr
                    ;
logical_and_expr : equal_relation_expr
                 | logical_and_expr AND equal_relation_expr
                 ;
logical_or_expr : logical_and_expr
                | logical_or_expr OR logical_and_expr
                ;
const_expr : add_sub_expr
           ;


%%

void yyerror(const char* s) {
    err("parser") << s << std::endl;
    std::exit(1);
}
