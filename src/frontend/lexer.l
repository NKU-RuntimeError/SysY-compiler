%option noyywrap
%top{
#include <iomanip>
#include "log.h"
#include "parser.h"
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
    // log("lexer") << "P_WHITESPACE" << std::endl;
}
{P_BLOCK_COMMENT} {
    // log("lexer") << "P_BLOCK_COMMENT" << std::endl;
}
{P_LINE_COMMENT} {
    // log("lexer") << "P_LINE_COMMENT" << std::endl;
}



"const" {
    log("lexer") << "CONST" << std::endl;
    return CONST;
}
"int" {
    log("lexer") << "TYPE_INT" << std::endl;
    return TYPE_INT;
}
"float" {
    log("lexer") << "TYPE_FLOAT" << std::endl;
    return TYPE_FLOAT;
}
"void" {
    log("lexer") << "TYPE_VOID" << std::endl;
    return TYPE_VOID;
}
"if" {
    log("lexer") << "IF" << std::endl;
    return IF;
}
"else" {
    log("lexer") << "ELSE" << std::endl;
    return ELSE;
}
"while" {
    log("lexer") << "WHILE" << std::endl;
    return WHILE;
}
"break" {
    log("lexer") << "BREAK" << std::endl;
    return BREAK;
}
"continue" {
    log("lexer") << "CONTINUE" << std::endl;
    return CONTINUE;
}
"return" {
    log("lexer") << "RETURN" << std::endl;
    return RETURN;
}



"&&" {
    log("lexer") << "AND" << std::endl;
    return AND;
}
"||" {
    log("lexer") << "OR" << std::endl;
    return OR;
}



"<=" {
    log("lexer") << "LE" << std::endl;
    return LE;
}
">=" {
    log("lexer") << "GE" << std::endl;
    return GE;
}
"==" {
    log("lexer") << "EQ" << std::endl;
    return EQ;
}
"!=" {
    log("lexer") << "NE" << std::endl;
    return NE;
}
"<" {
    log("lexer") << "LT" << std::endl;
    return LT;
}
">" {
    log("lexer") << "GT" << std::endl;
    return GT;
}



"+" {
    log("lexer") << "PLUS" << std::endl;
    return PLUS;
}
"-" {
    log("lexer") << "MINUS" << std::endl;
    return MINUS;
}
"!" {
    log("lexer") << "NOT" << std::endl;
    return NOT;
}
"*" {
    log("lexer") << "MUL" << std::endl;
    return MUL;
}
"/" {
    log("lexer") << "DIV" << std::endl;
    return DIV;
}
"%" {
    log("lexer") << "MOD" << std::endl;
    return MOD;
}



"=" {
    log("lexer") << "ASSIGN" << std::endl;
    return ASSIGN;
}
"," {
    log("lexer") << "COMMA" << std::endl;
    return COMMA;
}
";" {
    log("lexer") << "SEMICOLON" << std::endl;
    return SEMICOLON;
}



"{" {
    log("lexer") << "LBRACE" << std::endl;
    return LBRACE;
}
"}" {
    log("lexer") << "RBRACE" << std::endl;
    return RBRACE;
}
"[" {
    log("lexer") << "LBRACKET" << std::endl;
    return LBRACKET;
}
"]" {
    log("lexer") << "RBRACKET" << std::endl;
    return RBRACKET;
}
"(" {
    log("lexer") << "LPAREN" << std::endl;
    return LPAREN;
}
")" {
    log("lexer") << "RPAREN" << std::endl;
    return RPAREN;
}



{P_VALUE_FLOAT_DEC} {
    double value = std::stod(yytext);
    log("lexer") << "VALUE_FLOAT" << std::setw(10) << value << std::endl;
    return VALUE_FLOAT;
}
{P_VALUE_FLOAT_HEX} {
    double value = std::stod(yytext);
    log("lexer") << "VALUE_FLOAT" << std::setw(10) << value << std::endl;
    return VALUE_FLOAT;
}
{P_VALUE_INT} {
    int value = std::stoi(yytext);
    log("lexer") << "VALUE_INT" << std::setw(10) << value << std::endl;
    return VALUE_INT;
}
{P_IDENTIFIER} {
    std::string identifier = yytext;
    log("lexer") << "IDENTIFIER" << std::setw(10) << identifier << std::endl;
    return IDENTIFIER;
}

%%