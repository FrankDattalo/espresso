#include "ecomp.hh"
#include "ert.hh"
#include <cctype>

namespace espresso {

namespace compiler {

template<typename T, std::int64_t N>
class Deque {
public:
    Deque()
    : start{0}
    , end{0}
    {
        static_assert(N > 0);
    }

    ~Deque() = default;

    void Put(T* src) {
        if (Length() == N) {
            Panic("Deque is full");
        }
        *at(end) = *src;
        end++;
    }

    T Take() {
        if (Length() == 0) {
            Panic("Deque is empty");
        }
        T result = *at(start);
        start++;
        return result;
    }

    std::int64_t Length() const {
        return end - start;
    }


private:
    T* at(std::int64_t idx) {
        return &items[idx % N];
    }

    T items[N];
    std::int64_t start;
    std::int64_t end;
};

/**
 * 
 * Grammar:
 * 
 * file
 *  | statement*
 * 
 * statement
 *  | "global" identifier "=" expression ";"
 *  | "var" identifier "=" expression ";"
 *  | "if" expression "{" statement* "}" ("else" "{" statement* "}")?
 *  | "while" expression "{" statement* "}"
 *  | "return" (expression)? ";"
 *  | expression ";"
 * 
 * expression
 *  | float
 *  | integer
 *  | "\"" any "\""
 *  | "true"
 *  | "false"
 *  | "nil"
 *  | "fn" "(" ( identifier ("," identifier )* )? ")" "{" statement* "}"
 *  | invoke
 * 
 * invoke
 *  | target "(" ( expression ( "," expression )* )? ")"
 *
 * target
 *  | identifier
 *  | target "." identifier
 *  | "(" expression ")"
 * 
*/

struct Compiler {

    enum class TokenType {
        Global,
        Var,
        If,
        While,
        Return,
        Double,
        Integer,
        String,
        Boolean,
        Nil,
        Fn,
        Assign,
        Identifier,
        LeftParen,
        RightParen,
        Period,
        LeftCurly,
        RightCurly,
        SemiColon,
    };

    template<TokenType N>
    constexpr const char* TokenTypeToString() {
        switch (N) {
            case TokenType::Global: { return "Global"; }
            case TokenType::Var: { return "Var"; }
            case TokenType::If: { return "If"; }
            case TokenType::While: { return "While"; }
            case TokenType::Return: { return "Return"; }
            case TokenType::Double: { return "Double"; }
            case TokenType::Integer: { return "Integer"; }
            case TokenType::String: { return "String"; }
            case TokenType::Boolean: { return "Boolean"; }
            case TokenType::Nil: { return "Nil"; }
            case TokenType::Fn: { return "Fn"; }
            case TokenType::Assign: { return "Assign"; }
            case TokenType::Identifier: { return "Identifier"; }
            case TokenType::LeftParen: { return "LeftParen"; }
            case TokenType::RightParen: { return "RightParen"; }
            case TokenType::Period: { return "Period"; }
            case TokenType::LeftCurly: { return "LeftCurly"; }
            case TokenType::RightCurly: { return "RightCurly"; }
            case TokenType::SemiColon: { return "SemiColon"; }
            default: {
                static_assert(false);
            }
        }
    }

    struct Token {
        TokenType type;
        const char* source;
        std::int64_t length;
    };

    Runtime* rt;
    const char* string;
    std::int64_t length;
    std::int64_t index;
    Function* fn;

    void Compile() {
        while (!AtEof()) {
            CompileStatement();
        }
    }

    void CompileStatement() {
        Token current = Next();
        switch (current.type) {
            case TokenType::Var: {
                PutBack(&current);
                CompileVarStatement();
                break;
            }
            case TokenType::Global: {
                PutBack(&current);
                CompileGlobalStatement();
                break;
            }
            case TokenType::While: {
                PutBack(&current);
                CompileWhileStatement();
                break;
            }
            case TokenType::Return: {
                PutBack(&current);
                CompileWhileStatement();
                break;
            }
            default: {
                PutBack(&current);
                CompileExpressionStatement();
                break;
            }
        }
    }

    void CompileWhileStatement() {
    }

    void CompileExpressionStatement() {

    }

    void CompileGlobalStatement() {

    }

    void CompileVarStatement() {
        Expect<TokenType::Var>();
        Expect<TokenType::Identifier>();
        Expect<TokenType::Assign>();
    }

    void CompileExpression() {

    }

    template<TokenType T>
    Token Expect() {
        Token result = Next();
        if (result.type != T) {
            
            Abort("Expected a " TokenTypeToString(T));
        }
    }

    Deque<Token, 2> tokenBuffer;

    void PutBack(Token* token) {
        tokenBuffer.Put(token);
    }

    Token Next() {
        if (tokenBuffer.Length() > 0) {
            return tokenBuffer.Take();
        }
        Panic("TODO");
    }

    void Abort(const char* message) {
        rt->Local(Integer{0})->SetString(rt->NewString(message));
        rt->Throw(Integer{0});
        return;
    }

    bool AtEof() {
        while (index < length && std::isblank(string[index])) {
            index++;
        }
        return index >= length;
    }
};

void Compile(Runtime* rt) {
    rt->Local(Integer{0})->SetFunction(rt->NewFunction());
    Function* dest = rt->Local(Integer{0})->GetFunction(rt);
    String* source = rt->Local(Integer{1})->GetString(rt);

    Compiler compiler{
        rt,
        source->RawPointer(),
        source->Length().Unwrap(),
        0,
        dest,
    };

    compiler.Compile();
}

} // namespace compiler

} // namespace espresso
