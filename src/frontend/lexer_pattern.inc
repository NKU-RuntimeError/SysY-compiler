#define P_BLOCK_COMMENT         R"(\/\*(.|\r|\n)*?\*\/)"
#define P_LINE_COMMENT          R"(\/\/.*(\r\n|\r|\n))"
#define P_WHITESPACE            R"([\t ]+)"
#define P_NEWLINE               R"(\r\n|\r|\n)"
#define P_IDENTIFIER            R"([A-Za-z_][A-Za-z0-9_]*)"
#define P_VALUE_INT             R"((0[Xx][0-9A-Fa-f]+)|(0[0-7]+)|([1-9]\d*|0))"
#define P_VALUE_FLOAT_DEC       R"((((\d*\.\d+)|(\d+\.))([Ee][+-]?\d+)?)|(\d+([Ee][+-]?\d+)))"
#define P_VALUE_FLOAT_HEX       R"(0[Xx]((([0-9A-Fa-f]*\.[0-9A-Fa-f]+)|([0-9A-Fa-f]+\.))([Pp][+-]?\d+)|[0-9A-Fa-f]+([Pp][+-]?\d+)))"
#define P_VALUE_FLOAT           "(" P_VALUE_FLOAT_DEC ")|(" P_VALUE_FLOAT_HEX ")"

#define T_CALLBACK [](const std::string &str) -> std::optional<int>

static Pattern patterns[]{
        {P_NEWLINE,       T_CALLBACK {
            return std::nullopt;
        }},
        {P_WHITESPACE,    T_CALLBACK {
            return std::nullopt;
        }},
        {P_BLOCK_COMMENT, T_CALLBACK {
            Lexer::log("BLOCK_COMMENT");
            return std::nullopt;
        }},
        {P_LINE_COMMENT,  T_CALLBACK {
            Lexer::log("LINE_COMMENT");
            return std::nullopt;
        }},

        {R"(const\b)",    T_CALLBACK {
            Lexer::log("CONST", str);
            return CONST;
        }},

        {R"(int\b)",      T_CALLBACK {
            Lexer::log("TYPE_INT", str);
            return TYPE_INT;
        }},
        {R"(float\b)",    T_CALLBACK {
            Lexer::log("TYPE_FLOAT", str);
            return TYPE_FLOAT;
        }},
        {R"(void\b)",     T_CALLBACK {
            Lexer::log("TYPE_VOID", str);
            return TYPE_VOID;
        }},
        {R"(if\b)",       T_CALLBACK {
            Lexer::log("IF", str);
            return IF;
        }},
        {R"(else\b)",     T_CALLBACK {
            Lexer::log("ELSE", str);
            return ELSE;
        }},
        {R"(while\b)",    T_CALLBACK {
            Lexer::log("WHILE", str);
            return WHILE;
        }},
        {R"(break\b)",    T_CALLBACK {
            Lexer::log("BREAK", str);
            return BREAK;
        }},
        {R"(continue\b)", T_CALLBACK {
            Lexer::log("CONTINUE", str);
            return CONTINUE;
        }},
        {R"(return\b)",   T_CALLBACK {
            Lexer::log("RETURN", str);
            return RETURN;
        }},

        {"&&",            T_CALLBACK {
            Lexer::log("AND", str);
            return AND;
        }},
        {R"(\|\|)",       T_CALLBACK {
            Lexer::log("OR", str);
            return OR;
        }},
        {"<=",            T_CALLBACK {
            Lexer::log("LE", str);
            return LE;
        }},
        {">=",            T_CALLBACK {
            Lexer::log("GE", str);
            return GE;
        }},
        {"==",            T_CALLBACK {
            Lexer::log("EQ", str);
            return EQ;
        }},
        {"!=",            T_CALLBACK {
            Lexer::log("NE", str);
            return NE;
        }},
        {"<",             T_CALLBACK {
            Lexer::log("LT", str);
            return LT;
        }},
        {">",             T_CALLBACK {
            Lexer::log("GT", str);
            return GT;
        }},
        {R"(\+)",         T_CALLBACK {
            Lexer::log("ADD", str);
            return PLUS;
        }},
        {"-",             T_CALLBACK {
            Lexer::log("SUB", str);
            return MINUS;
        }},
        {"!",             T_CALLBACK {
            Lexer::log("NOT", str);
            return NOT;
        }},
        {R"(\*)",         T_CALLBACK {
            Lexer::log("MUL", str);
            return MUL;
        }},
        {R"(\/)",         T_CALLBACK {
            Lexer::log("DIV", str);
            return DIV;
        }},
        {"%",             T_CALLBACK {
            Lexer::log("MOD", str);
            return MOD;
        }},
        {"=",             T_CALLBACK {
            Lexer::log("ASSIGN", str);
            return ASSIGN;
        }},
        {",",             T_CALLBACK {
            Lexer::log("COMMA", str);
            return COMMA;
        }},
        {";",             T_CALLBACK {
            Lexer::log("SEMICOLON", str);
            return SEMICOLON;
        }},
        {R"(\{)",         T_CALLBACK {
            Lexer::log("LBRACE", str);
            return LBRACE;
        }},
        {R"(\})",         T_CALLBACK {
            Lexer::log("RBRACE", str);
            return RBRACE;
        }},
        {R"(\[)",         T_CALLBACK {
            Lexer::log("LBRACKET", str);
            return LBRACKET;
        }},
        {R"(\])",         T_CALLBACK {
            Lexer::log("RBRACKET", str);
            return RBRACKET;
        }},
        {R"(\()",         T_CALLBACK {
            Lexer::log("LPAREN", str);
            return LPAREN;
        }},
        {R"(\))",         T_CALLBACK {
            Lexer::log("RPAREN", str);
            return RPAREN;
        }},

        {P_VALUE_FLOAT,   T_CALLBACK {
            yylval.floatType = std::stof(str);
            Lexer::log("VALUE_FLOAT", str);
            return VALUE_FLOAT;
        }},
        {P_VALUE_INT,     T_CALLBACK {
            yylval.intType = std::stoul(str, nullptr, 0);
            Lexer::log("VALUE_INT", str);
            return VALUE_INT;
        }},
        {P_IDENTIFIER,    T_CALLBACK {
            yylval.strType = WithPosition(
                    Memory::make<std::string>(str),
                    {currRow, currCol}
            );
            Lexer::log("IDENTIFIER", str);
            return IDENTIFIER;
        }},


        {".+",            T_CALLBACK {
            throw std::runtime_error(
                    "Unknown token: " + str + "at " +
                    std::to_string(currRow) + ":" + std::to_string(currCol)
            );
        }}
};

#undef T_CALLBACK
