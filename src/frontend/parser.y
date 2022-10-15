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

Program : Any Program
	| Any
	;

Any : CONST
    | COMMA
    | SEMICOLON
    | TYPE_INT
    | TYPE_FLOAT
    | TYPE_VOID
    | IDENTIFIER
    | LBRACE
    | RBRACE
    | LBRACKET
    | RBRACKET
    | LPAREN
    | RPAREN
    | ASSIGN
    | IF
    | ELSE
    | WHILE
    | BREAK
    | CONTINUE
    | RETURN
    | VALUE_INT
    | VALUE_FLOAT
    | PLUS
    | MINUS
    | NOT
    | MUL
    | DIV
    | MOD
    | LT
    | LE
    | GT
    | GE
    | EQ
    | NE
    | AND
    | OR
    ;

%%

void yyerror(const char* s) {
    err("parser") << s << std::endl;
    std::exit(1);
}
