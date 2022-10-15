#define P_BLOCK_COMMENT         R"(\/\*(.|\r|\n)*\*\/)"
#define P_LINE_COMMENT          R"(\/\/.*)"
#define P_WHITESPACE            R"(\s+)"
#define P_IDENTIFIER            R"([A-Za-z_][A-Za-z0-9_]*)"
#define P_VALUE_INT             R"((0[Xx][0-9A-Fa-f]+)|(0[0-7]+)|([1-9]\d*|0))"
#define P_VALUE_FLOAT_DEC       R"((((\d*\.\d+)|(\d+\.))([Ee][+-]?\d+)?)|(\d+([Ee][+-]?\d+)))"
#define P_VALUE_FLOAT_HEX       R"(0[Xx]((([0-9A-Fa-f]*\.[0-9A-Fa-f]+)|([0-9A-Fa-f]+\.))([Pp][+-]?\d+)|[0-9A-Fa-f]+([Pp][+-]?\d+)))"

#define T_CALLBACK [](const std::string &str) -> std::optional<int>

static Pattern patterns[]{
        {P_WHITESPACE,      T_CALLBACK {
            return std::nullopt;
        }},
        {P_BLOCK_COMMENT,   T_CALLBACK {
            return std::nullopt;
        }},
        {P_LINE_COMMENT,    T_CALLBACK {
            return std::nullopt;
        }},

        {"const",           T_CALLBACK {
            log("lexer") << "CONST" << std::endl;
            return CONST;
        }},

        {"int",             T_CALLBACK {
            log("lexer") << "TYPE_INT" << std::endl;
            return TYPE_INT;
        }},
        {"float",           T_CALLBACK {
            log("lexer") << "TYPE_FLOAT" << std::endl;
            return TYPE_FLOAT;
        }},
        {"void",            T_CALLBACK {
            log("lexer") << "TYPE_VOID" << std::endl;
            return TYPE_VOID;
        }},
        {"if",              T_CALLBACK {
            log("lexer") << "IF" << std::endl;
            return IF;
        }},
        {"else",            T_CALLBACK {
            log("lexer") << "ELSE" << std::endl;
            return ELSE;
        }},
        {"while",           T_CALLBACK {
            log("lexer") << "WHILE" << std::endl;
            return WHILE;
        }},
        {"break",           T_CALLBACK {
            log("lexer") << "BREAK" << std::endl;
            return BREAK;
        }},
        {"continue",        T_CALLBACK {
            log("lexer") << "CONTINUE" << std::endl;
            return CONTINUE;
        }},
        {"return",          T_CALLBACK {
            log("lexer") << "RETURN" << std::endl;
            return RETURN;
        }},

        {"&&",              T_CALLBACK {
            log("lexer") << "AND" << std::endl;
            return AND;
        }},
        {R"(\|\|)",         T_CALLBACK {
            log("lexer") << "OR" << std::endl;
            return OR;
        }},
        {"<=",              T_CALLBACK {
            log("lexer") << "LE" << std::endl;
            return LE;
        }},
        {">=",              T_CALLBACK {
            log("lexer") << "GE" << std::endl;
            return GE;
        }},
        {"==",              T_CALLBACK {
            log("lexer") << "EQ" << std::endl;
            return EQ;
        }},
        {"!=",              T_CALLBACK {
            log("lexer") << "NE" << std::endl;
            return NE;
        }},
        {"<",               T_CALLBACK {
            log("lexer") << "LT" << std::endl;
            return LT;
        }},
        {">",               T_CALLBACK {
            log("lexer") << "GT" << std::endl;
            return GT;
        }},
        {R"(\+)",           T_CALLBACK {
            log("lexer") << "ADD" << std::endl;
            return PLUS;
        }},
        {"-",               T_CALLBACK {
            log("lexer") << "SUB" << std::endl;
            return MINUS;
        }},
        {"!",               T_CALLBACK {
            log("lexer") << "NOT" << std::endl;
            return NOT;
        }},
        {R"(\*)",           T_CALLBACK {
            log("lexer") << "MUL" << std::endl;
            return MUL;
        }},
        {R"(\/)",           T_CALLBACK {
            log("lexer") << "DIV" << std::endl;
            return DIV;
        }},
        {"%",               T_CALLBACK {
            log("lexer") << "MOD" << std::endl;
            return MOD;
        }},
        {"=",               T_CALLBACK {
            log("lexer") << "ASSIGN" << std::endl;
            return ASSIGN;
        }},
        {",",               T_CALLBACK {
            log("lexer") << "COMMA" << std::endl;
            return COMMA;
        }},
        {";",               T_CALLBACK {
            log("lexer") << "SEMICOLON" << std::endl;
            return SEMICOLON;
        }},
        {R"(\{)",           T_CALLBACK {
            log("lexer") << "LBRACE" << std::endl;
            return LBRACE;
        }},
        {R"(\})",           T_CALLBACK {
            log("lexer") << "RBRACE" << std::endl;
            return RBRACE;
        }},
        {R"(\[)",           T_CALLBACK {
            log("lexer") << "LBRACKET" << std::endl;
            return LBRACKET;
        }},
        {R"(\])",           T_CALLBACK {
            log("lexer") << "RBRACKET" << std::endl;
            return RBRACKET;
        }},
        {R"(\()",           T_CALLBACK {
            log("lexer") << "LPAREN" << std::endl;
            return LPAREN;
        }},
        {R"(\))",           T_CALLBACK {
            log("lexer") << "RPAREN" << std::endl;
            return RPAREN;
        }},

        {P_VALUE_FLOAT_HEX, T_CALLBACK {
            log("lexer") << "VALUE_FLOAT_HEX" << std::endl;
            return VALUE_FLOAT;
        }},
        {P_VALUE_FLOAT_DEC, T_CALLBACK {
            log("lexer") << "VALUE_FLOAT_DEC" << std::endl;
            return VALUE_FLOAT;
        }},
        {P_VALUE_INT,       T_CALLBACK {
            log("lexer") << "VALUE_INT" << std::endl;
            return VALUE_INT;
        }},
        {P_IDENTIFIER,      T_CALLBACK {
            log("lexer") << "IDENTIFIER" << std::endl;
            return IDENTIFIER;
        }},


        {".+",              T_CALLBACK {
            err("lexer") << "Unknown token: " << str << std::endl;
            std::exit(1);
        }}
};

#undef T_CALLBACK
