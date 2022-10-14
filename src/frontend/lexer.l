%option noyywrap
%top{
#include <iomanip>
#include "log.h"
#include "parser.h"
#define LOG_LEXER "lexer"
}

P_BLOCK_COMMENT         \/\*(.|\r|\n)*\*\/
P_LINE_COMMENT          \/\/.*(\r\n|\r|\n)
P_WHITESPACE            (\s|[ ]|\r|\n)*
P_IDENTIFIER            [A-Za-z_][A-Za-z0-9_]*
P_VALUE_INT             ([1-9]\d*|0)|(0[Xx][0-9A-Fa-f]+)|(0[0-7]+)
P_VALUE_FLOAT_DEC       (((\d*\.\d+)|(\d+\.))([Ee][+-]?\d+)?)|(\d+([Ee][+-]?\d+))
P_VALUE_FLOAT_HEX       0[Xx]((([0-9A-Fa-f]*\.[0-9A-Fa-f]+)|([0-9A-Fa-f]+\.))([Pp][+-]?\d+)|[0-9A-Fa-f]+([Pp][+-]?\d+))

%%

{P_WHITESPACE} {
    // log(LOG_LEXER) << "P_WHITESPACE" << std::endl;
}
{P_BLOCK_COMMENT} {
    // log(LOG_LEXER) << "P_BLOCK_COMMENT" << std::endl;
}
{P_LINE_COMMENT} {
    // log(LOG_LEXER) << "P_LINE_COMMENT" << std::endl;
}



"const" {
    log(LOG_LEXER) << "CONST" << std::endl;
    return CONST;
}
"int" {
    log(LOG_LEXER) << "TYPE_INT" << std::endl;
    return TYPE_INT;
}
"float" {
    log(LOG_LEXER) << "TYPE_FLOAT" << std::endl;
    return TYPE_FLOAT;
}
"void" {
    log(LOG_LEXER) << "TYPE_VOID" << std::endl;
    return TYPE_VOID;
}
"if" {
    log(LOG_LEXER) << "IF" << std::endl;
    return IF;
}
"else" {
    log(LOG_LEXER) << "ELSE" << std::endl;
    return ELSE;
}
"while" {
    log(LOG_LEXER) << "WHILE" << std::endl;
    return WHILE;
}
"break" {
    log(LOG_LEXER) << "BREAK" << std::endl;
    return BREAK;
}
"continue" {
    log(LOG_LEXER) << "CONTINUE" << std::endl;
    return CONTINUE;
}
"return" {
    log(LOG_LEXER) << "RETURN" << std::endl;
    return RETURN;
}



"&&" {
    log(LOG_LEXER) << "AND" << std::endl;
    return AND;
}
"||" {
    log(LOG_LEXER) << "OR" << std::endl;
    return OR;
}



"<=" {
    log(LOG_LEXER) << "LE" << std::endl;
    return LE;
}
">=" {
    log(LOG_LEXER) << "GE" << std::endl;
    return GE;
}
"==" {
    log(LOG_LEXER) << "EQ" << std::endl;
    return EQ;
}
"!=" {
    log(LOG_LEXER) << "NE" << std::endl;
    return NE;
}
"<" {
    log(LOG_LEXER) << "LT" << std::endl;
    return LT;
}
">" {
    log(LOG_LEXER) << "GT" << std::endl;
    return GT;
}



"+" {
    log(LOG_LEXER) << "PLUS" << std::endl;
    return PLUS;
}
"-" {
    log(LOG_LEXER) << "MINUS" << std::endl;
    return MINUS;
}
"!" {
    log(LOG_LEXER) << "NOT" << std::endl;
    return NOT;
}
"*" {
    log(LOG_LEXER) << "MUL" << std::endl;
    return MUL;
}
"/" {
    log(LOG_LEXER) << "DIV" << std::endl;
    return DIV;
}
"%" {
    log(LOG_LEXER) << "MOD" << std::endl;
    return MOD;
}



"=" {
    log(LOG_LEXER) << "ASSIGN" << std::endl;
    return ASSIGN;
}
"," {
    log(LOG_LEXER) << "COMMA" << std::endl;
    return COMMA;
}
";" {
    log(LOG_LEXER) << "SEMICOLON" << std::endl;
    return SEMICOLON;
}



"{" {
    log(LOG_LEXER) << "LBRACE" << std::endl;
    return LBRACE;
}
"}" {
    log(LOG_LEXER) << "RBRACE" << std::endl;
    return RBRACE;
}
"[" {
    log(LOG_LEXER) << "LBRACKET" << std::endl;
    return LBRACKET;
}
"]" {
    log(LOG_LEXER) << "RBRACKET" << std::endl;
    return RBRACKET;
}
"(" {
    log(LOG_LEXER) << "LPAREN" << std::endl;
    return LPAREN;
}
")" {
    log(LOG_LEXER) << "RPAREN" << std::endl;
    return RPAREN;
}

{P_VALUE_FLOAT_DEC} {
    log(LOG_LEXER) << "VALUE_FLOAT" << std::endl;
    return VALUE_FLOAT;
}
{P_VALUE_FLOAT_HEX} {
    log(LOG_LEXER) << "VALUE_FLOAT" << std::endl;
    return VALUE_FLOAT;
}
{P_VALUE_INT} {
    int value = std::stoi(yytext);
    log(LOG_LEXER) << "VALUE_INT" << std::setw(10) << value << std::endl;

    return VALUE_INT;
}
{P_IDENTIFIER} {
    log(LOG_LEXER) << "IDENTIFIER" << std::endl;
    return IDENTIFIER;
}

%%