#define P_BLOCK_COMMENT         R"(\/\*(.|\r|\n)*?\*\/)"
#define P_LINE_COMMENT          R"(\/\/.*(\r\n|\r|\n))"
#define P_WHITESPACE            R"([\t ]+)"
#define P_NEWLINE               R"(\r\n|\r|\n)"
#define P_IDENTIFIER            R"([A-Za-z_][A-Za-z0-9_]*)"
#define P_VALUE_INT             R"((0[Xx][0-9A-Fa-f]+)|(0[0-7]+)|([1-9]\d*|0))"
#define P_VALUE_FLOAT_DEC       R"((((\d*\.\d+)|(\d+\.))([Ee][+-]?\d+)?)|(\d+([Ee][+-]?\d+)))"
#define P_VALUE_FLOAT_HEX       R"(0[Xx]((([0-9A-Fa-f]*\.[0-9A-Fa-f]+)|([0-9A-Fa-f]+\.))([Pp][+-]?\d+)|[0-9A-Fa-f]+([Pp][+-]?\d+)))"
#define P_VALUE_FLOAT           "(" P_VALUE_FLOAT_DEC ")|(" P_VALUE_FLOAT_HEX ")"

inline static void lexerLog(
        const std::string &token,
        const std::string &lexeme = "",
        void *ptr = nullptr
) {
    auto &stream = log("lexer");
    stream << std::setw(20) << token <<
           std::setw(20) << lexeme <<
           std::setw(10) << currRow <<
           std::setw(10) << currCol;
    if (ptr) {
        stream << std::setw(20) << ptr;
    }
    stream << std::endl;
}

#define T_CALLBACK [](const std::string &str) -> std::optional<int>

static Pattern patterns[]{
        {P_NEWLINE,       T_CALLBACK {
            currRow++;
            currCol = 1;
            return std::nullopt;
        }},
        {P_WHITESPACE,    T_CALLBACK {
            currCol += str.length();
            return std::nullopt;
        }},
        {P_BLOCK_COMMENT, T_CALLBACK {
            lexerLog("BLOCK_COMMENT");
            size_t newLineCount = std::count_if(
                    str.begin(),
                    str.end(),
                    [](char c) { return c == '\n'; }
            );
            if (newLineCount > 0) {
                currRow += newLineCount;
                currCol = 1;
            } else {
                currCol += str.length();
            }
            return std::nullopt;
        }},
        {P_LINE_COMMENT,  T_CALLBACK {
            lexerLog("LINE_COMMENT");
            currRow++;
            currCol = 1;
            return std::nullopt;
        }},

        {R"(const\b)",    T_CALLBACK {
            lexerLog("CONST", str);
            currCol += str.length();
            return CONST;
        }},

        {R"(int\b)",      T_CALLBACK {
            lexerLog("TYPE_INT", str);
            currCol += str.length();
            return TYPE_INT;
        }},
        {R"(float\b)",    T_CALLBACK {
            lexerLog("TYPE_FLOAT", str);
            currCol += str.length();
            return TYPE_FLOAT;
        }},
        {R"(void\b)",     T_CALLBACK {
            lexerLog("TYPE_VOID", str);
            currCol += str.length();
            return TYPE_VOID;
        }},
        {R"(if\b)",       T_CALLBACK {
            lexerLog("IF", str);
            currCol += str.length();
            return IF;
        }},
        {R"(else\b)",     T_CALLBACK {
            lexerLog("ELSE", str);
            currCol += str.length();
            return ELSE;
        }},
        {R"(while\b)",    T_CALLBACK {
            lexerLog("WHILE", str);
            currCol += str.length();
            return WHILE;
        }},
        {R"(break\b)",    T_CALLBACK {
            lexerLog("BREAK", str);
            currCol += str.length();
            return BREAK;
        }},
        {R"(continue\b)", T_CALLBACK {
            lexerLog("CONTINUE", str);
            currCol += str.length();
            return CONTINUE;
        }},
        {R"(return\b)",   T_CALLBACK {
            lexerLog("RETURN", str);
            currCol += str.length();
            return RETURN;
        }},

        {"&&",            T_CALLBACK {
            lexerLog("AND", str);
            currCol += str.length();
            return AND;
        }},
        {R"(\|\|)",       T_CALLBACK {
            lexerLog("OR", str);
            currCol += str.length();
            return OR;
        }},
        {"<=",            T_CALLBACK {
            lexerLog("LE", str);
            currCol += str.length();
            return LE;
        }},
        {">=",            T_CALLBACK {
            lexerLog("GE", str);
            currCol += str.length();
            return GE;
        }},
        {"==",            T_CALLBACK {
            lexerLog("EQ", str);
            currCol += str.length();
            return EQ;
        }},
        {"!=",            T_CALLBACK {
            lexerLog("NE", str);
            currCol += str.length();
            return NE;
        }},
        {"<",             T_CALLBACK {
            lexerLog("LT", str);
            currCol += str.length();
            return LT;
        }},
        {">",             T_CALLBACK {
            lexerLog("GT", str);
            currCol += str.length();
            return GT;
        }},
        {R"(\+)",         T_CALLBACK {
            lexerLog("ADD", str);
            currCol += str.length();
            return PLUS;
        }},
        {"-",             T_CALLBACK {
            lexerLog("SUB", str);
            currCol += str.length();
            return MINUS;
        }},
        {"!",             T_CALLBACK {
            lexerLog("NOT", str);
            currCol += str.length();
            return NOT;
        }},
        {R"(\*)",         T_CALLBACK {
            lexerLog("MUL", str);
            currCol += str.length();
            return MUL;
        }},
        {R"(\/)",         T_CALLBACK {
            lexerLog("DIV", str);
            currCol += str.length();
            return DIV;
        }},
        {"%",             T_CALLBACK {
            lexerLog("MOD", str);
            currCol += str.length();
            return MOD;
        }},
        {"=",             T_CALLBACK {
            lexerLog("ASSIGN", str);
            currCol += str.length();
            return ASSIGN;
        }},
        {",",             T_CALLBACK {
            lexerLog("COMMA", str);
            currCol += str.length();
            return COMMA;
        }},
        {";",             T_CALLBACK {
            lexerLog("SEMICOLON", str);
            currCol += str.length();
            return SEMICOLON;
        }},
        {R"(\{)",         T_CALLBACK {
            lexerLog("LBRACE", str);
            currCol += str.length();
            return LBRACE;
        }},
        {R"(\})",         T_CALLBACK {
            lexerLog("RBRACE", str);
            currCol += str.length();
            return RBRACE;
        }},
        {R"(\[)",         T_CALLBACK {
            lexerLog("LBRACKET", str);
            currCol += str.length();
            return LBRACKET;
        }},
        {R"(\])",         T_CALLBACK {
            lexerLog("RBRACKET", str);
            currCol += str.length();
            return RBRACKET;
        }},
        {R"(\()",         T_CALLBACK {
            lexerLog("LPAREN", str);
            currCol += str.length();
            return LPAREN;
        }},
        {R"(\))",         T_CALLBACK {
            lexerLog("RPAREN", str);
            currCol += str.length();
            return RPAREN;
        }},

        {P_VALUE_FLOAT,   T_CALLBACK {
            yylval.floatType = std::stof(str);
            lexerLog("VALUE_FLOAT", str);
            currCol += str.length();
            return VALUE_FLOAT;
        }},
        {P_VALUE_INT,     T_CALLBACK {
            yylval.intType = std::stoul(str, nullptr, 0);
            lexerLog("VALUE_INT", str);
            currCol += str.length();
            return VALUE_INT;
        }},
        {P_IDENTIFIER,    T_CALLBACK {
            yylval.strType = Memory::make<std::string>(str);
            lexerLog("IDENTIFIER", str);
            currCol += str.length();
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
