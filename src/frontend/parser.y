%{
#include <iostream>
#include <string>
#include "log.h"
#include "lexer.h"
int yylex();
extern int yyparse();
void yyerror(const char* s);
%}

%token CONST
%token COMMA
%token SEMICOLON
%token TYPE_INT TYPE_FLOAT TYPE_VOID
%token IDENTIFIER
%token LBRACE RBRACE
%token LBRACKET RBRACKET
%token LPAREN RPAREN
%token ASSIGN
%token IF ELSE
%token WHILE BREAK CONTINUE
%token RETURN
%token VALUE_INT VALUE_FLOAT
%token PLUS MINUS NOT
%token MUL DIV MOD
%token LT LE GT GE EQ NE
%token AND OR

%%

compile_unit_opt : compile_unit
                 | /* empty */
                 ;
compile_unit : compile_unit compile_unit_element
             | compile_unit_element
             ;
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
const_var_def : IDENTIFIER ASSIGN const_expr
              | array ASSIGN const_initializer_list
              ;
array : array LBRACKET const_expr RBRACKET
      | IDENTIFIER LBRACKET const_expr RBRACKET
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
var_def : IDENTIFIER
        | IDENTIFIER ASSIGN expr
        | array
        | array ASSIGN initializer_list
        ;
initializer_list : LBRACE initializer_list_inner RBRACE
                 ;
initializer_list_inner : initializer_list_inner COMMA initializer_list_element
                       | initializer_list_element
                       | /* empty */
                       ;
initializer_list_element : expr
                         | initializer_list
                         ;
func_def : func_type IDENTIFIER LPAREN func_arg_list RPAREN block
         ;
func_type : TYPE_VOID
	  | TYPE_INT
	  | TYPE_FLOAT
          ;
func_arg_list : func_arg_list func_arg
              | func_arg
              | /* empty */
              ;
func_arg : var_type IDENTIFIER
         | var_type func_arg_array
         ;
func_arg_array : func_arg_array LBRACKET const_expr RBRACKET
               | IDENTIFIER LBRACKET RBRACKET
               ;
block : LBRACE block_inner RBRACE
      ;
block_inner : block_inner block_element
            | block_element
            ;
block_element : decl
              | stmt
              ;
stmt : lval ASSIGN expr SEMICOLON
     | expr SEMICOLON
     | block
     | IF LPAREN condition RPAREN stmt
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
func_param_list : func_param_list expr
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
