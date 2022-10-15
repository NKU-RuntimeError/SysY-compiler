#define P_BLOCK_COMMENT         R"(\/\*(.|\r|\n)*\*\/)"
#define P_LINE_COMMENT          R"(\/\/.*(\r\n|\r|\n))"
#define P_WHITESPACE            R"([\t ]+)"
#define P_NEWLINE               R"(\r\n|\r|\n)"
#define P_IDENTIFIER            R"([A-Za-z_][A-Za-z0-9_]*)"
#define P_VALUE_INT             R"((0[Xx][0-9A-Fa-f]+)|(0[0-7]+)|([1-9]\d*|0))"
#define P_VALUE_FLOAT_DEC       R"((((\d*\.\d+)|(\d+\.))([Ee][+-]?\d+)?)|(\d+([Ee][+-]?\d+)))"
#define P_VALUE_FLOAT_HEX       R"(0[Xx]((([0-9A-Fa-f]*\.[0-9A-Fa-f]+)|([0-9A-Fa-f]+\.))([Pp][+-]?\d+)|[0-9A-Fa-f]+([Pp][+-]?\d+)))"

inline static void lexerLog(const std::string& token, const std::string& lexeme) {
    log("lexer") <<
    std::setw(20) << token <<
    std::setw(20) << lexeme <<
    std::setw(10) << currLine <<
    std::setw(10) << currCol <<
    std::endl;
}

#define T_CALLBACK [](const std::string &str) -> std::optional<int>

static Pattern patterns[]{
        {P_NEWLINE,         T_CALLBACK {
            currLine++;
            currCol = 1;
            return std::nullopt;
        }},
        {P_WHITESPACE,      T_CALLBACK {
            currCol += str.length();
            return std::nullopt;
        }},
        {P_BLOCK_COMMENT,   T_CALLBACK {
            int newLineCount = std::count_if(
                    str.begin(),
                    str.end(),
                    [](char c) { return c == '\n'; }
            );
            if (newLineCount > 0) {
                currLine += newLineCount;
                currCol = 1;
            } else {
                currCol += str.length();
            }
            return std::nullopt;
        }},
        {P_LINE_COMMENT,    T_CALLBACK {
            currLine++;
            currCol = 1;
            return std::nullopt;
        }},

        {"const",           T_CALLBACK {
            lexerLog("CONST", str);
            currCol += str.length();
            return CONST;
        }},

        {"int",             T_CALLBACK {
            lexerLog("TYPE_INT", str);
            currCol += str.length();
            return TYPE_INT;
        }},
        {"float",           T_CALLBACK {
            lexerLog("TYPE_FLOAT", str);
            currCol += str.length();
            return TYPE_FLOAT;
        }},
        {"void",            T_CALLBACK {
            lexerLog("TYPE_VOID", str);
            currCol += str.length();
            return TYPE_VOID;
        }},
        {"if",              T_CALLBACK {
            lexerLog("IF", str);
            currCol += str.length();
            return IF;
        }},
        {"else",            T_CALLBACK {
            lexerLog("ELSE", str);
            currCol += str.length();
            return ELSE;
        }},
        {"while",           T_CALLBACK {
            lexerLog("WHILE", str);
            currCol += str.length();
            return WHILE;
        }},
        {"break",           T_CALLBACK {
            lexerLog("BREAK", str);
            currCol += str.length();
            return BREAK;
        }},
        {"continue",        T_CALLBACK {
            lexerLog("CONTINUE", str);
            currCol += str.length();
            return CONTINUE;
        }},
        {"return",          T_CALLBACK {
            lexerLog("RETURN", str);
            currCol += str.length();
            return RETURN;
        }},

        {"&&",              T_CALLBACK {
            lexerLog("AND", str);
            currCol += str.length();
            return AND;
        }},
        {R"(\|\|)",         T_CALLBACK {
            lexerLog("OR", str);
            currCol += str.length();
            return OR;
        }},
        {"<=",              T_CALLBACK {
            lexerLog("LE", str);
            currCol += str.length();
            return LE;
        }},
        {">=",              T_CALLBACK {
            lexerLog("GE", str);
            currCol += str.length();
            return GE;
        }},
        {"==",              T_CALLBACK {
            lexerLog("EQ", str);
            currCol += str.length();
            return EQ;
        }},
        {"!=",              T_CALLBACK {
            lexerLog("NE", str);
            currCol += str.length();
            return NE;
        }},
        {"<",               T_CALLBACK {
            lexerLog("LT", str);
            currCol += str.length();
            return LT;
        }},
        {">",               T_CALLBACK {
            lexerLog("GT", str);
            currCol += str.length();
            return GT;
        }},
        {R"(\+)",           T_CALLBACK {
            lexerLog("ADD", str);
            currCol += str.length();
            return PLUS;
        }},
        {"-",               T_CALLBACK {
            lexerLog("SUB", str);
            currCol += str.length();
            return MINUS;
        }},
        {"!",               T_CALLBACK {
            lexerLog("NOT", str);
            currCol += str.length();
            return NOT;
        }},
        {R"(\*)",           T_CALLBACK {
            lexerLog("MUL", str);
            currCol += str.length();
            return MUL;
        }},
        {R"(\/)",           T_CALLBACK {
            lexerLog("DIV", str);
            currCol += str.length();
            return DIV;
        }},
        {"%",               T_CALLBACK {
            lexerLog("MOD", str);
            currCol += str.length();
            return MOD;
        }},
        {"=",               T_CALLBACK {
            lexerLog("ASSIGN", str);
            currCol += str.length();
            return ASSIGN;
        }},
        {",",               T_CALLBACK {
            lexerLog("COMMA", str);
            currCol += str.length();
            return COMMA;
        }},
        {";",               T_CALLBACK {
            lexerLog("SEMICOLON", str);
            currCol += str.length();
            return SEMICOLON;
        }},
        {R"(\{)",           T_CALLBACK {
            lexerLog("LBRACE", str);
            currCol += str.length();
            return LBRACE;
        }},
        {R"(\})",           T_CALLBACK {
            lexerLog("RBRACE", str);
            currCol += str.length();
            return RBRACE;
        }},
        {R"(\[)",           T_CALLBACK {
            lexerLog("LBRACKET", str);
            currCol += str.length();
            return LBRACKET;
        }},
        {R"(\])",           T_CALLBACK {
            lexerLog("RBRACKET", str);
            currCol += str.length();
            return RBRACKET;
        }},
        {R"(\()",           T_CALLBACK {
            lexerLog("LPAREN", str);
            currCol += str.length();
            return LPAREN;
        }},
        {R"(\))",           T_CALLBACK {
            lexerLog("RPAREN", str);
            currCol += str.length();
            return RPAREN;
        }},

        {P_VALUE_FLOAT_HEX, T_CALLBACK {
            lexerLog("VALUE_FLOAT", str);
            currCol += str.length();
            return VALUE_FLOAT;
        }},
        {P_VALUE_FLOAT_DEC, T_CALLBACK {
            lexerLog("VALUE_FLOAT", str);
            currCol += str.length();
            return VALUE_FLOAT;
        }},
        {P_VALUE_INT,       T_CALLBACK {
            lexerLog("VALUE_INT", str);
            currCol += str.length();
            return VALUE_INT;
        }},
        {P_IDENTIFIER,      T_CALLBACK {
            lexerLog("IDENTIFIER", str);
            currCol += str.length();
            return IDENTIFIER;
        }},


        {".+",              T_CALLBACK {
            err("lexer") << "Unknown token: " << str << std::endl;
            std::exit(1);
        }}
};

#undef T_CALLBACK
