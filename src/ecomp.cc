#include "enat.hh"
#include "ert.hh"

namespace espresso {

namespace native {

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

    static void Abort(Runtime* runtime, const char* message) {
        runtime->Local(Integer{0})->SetString(runtime->NewString(message));
        runtime->Throw(Integer{0});
    }

    enum class TokenType {
        Global,
        Var,
        If,
        Else,
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
        EndOfFile,
        WhiteSpace,
        Unknown,
    };

    constexpr static const char* TokenTypeToString(TokenType type) {
        switch (type) {
            case TokenType::Global: { return "Global"; }
            case TokenType::Var: { return "Var"; }
            case TokenType::If: { return "If"; }
            case TokenType::Else: { return "Else"; }
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
            case TokenType::WhiteSpace: { return "WhiteSpace"; }
            case TokenType::EndOfFile: { return "EndOfFile"; }
            case TokenType::Unknown: { return "Unknown"; }
            default: {
                Panic("TokenTypeToString");
            }
        }
    }

    struct Token {
        TokenType type;
        const char* source;
        std::int64_t length;
    };

    struct Variable {
        Token token;
        bool isDefined;
    };

    struct Context {

        struct Scope {
            std::int64_t localsSize;
            std::int64_t stackSize;
        };

        std::int64_t argumentCount;
        std::int64_t registerCount;
        Vector<Variable> locals;
        Function* destination{nullptr};
        Vector<Scope> scopes;

        // [0, 256)
        constexpr static int64_t REGISTER_COUNT = 256;

        void Init(Runtime* runtime, Function* destination) {
            this->destination = destination;
            this->locals.Init(runtime);
            this->scopes.Init(runtime);
            Scope* scope = this->scopes.Push(runtime);
            // 1 for self
            scope->localsSize = 1;
            scope->stackSize = 0;
            argumentCount = 1;
            registerCount = 1;
            Variable* self = this->locals.Push(runtime);
            self->isDefined = true;
            constexpr const char* frameIdentifier = "self";
            static_assert(frameIdentifier[4] == '\0');
            self->token = Token{TokenType::Identifier, frameIdentifier, 4};
        }

        void DeInit(Runtime* runtime) {
            Integer nilConstant = NewNilConstant(runtime);
            EmitLong(runtime, ByteCodeType::LoadConstant, Integer{1}, nilConstant);
            Emit(runtime, ByteCodeType::Return, Integer{1});
            destination->SetStack(Integer{this->argumentCount}, Integer{this->registerCount});
            locals.DeInit(runtime);
            scopes.DeInit(runtime);
        }

        Integer Emit(Runtime* runtime, ByteCodeType byteCodeType, Integer reg1, Integer reg2, Integer reg3) {
            std::uint32_t result = 0;
            result |= static_cast<std::uint32_t>(byteCodeType);
            result |= bits::ARG1_BITS & (static_cast<std::uint32_t>(reg1.Unwrap()) << bits::ARG1_SHIFT);
            result |= bits::ARG2_BITS & (static_cast<std::uint32_t>(reg2.Unwrap()) << bits::ARG2_SHIFT);
            result |= bits::ARG3_BITS & (static_cast<std::uint32_t>(reg3.Unwrap()) << bits::ARG3_SHIFT);
            Integer id = destination->GetByteCodeCount();
            ByteCode* bc = destination->PushByteCode(runtime);
            bc->Init(runtime, result);
            return id;
        }

        Integer EmitLong(Runtime* runtime, ByteCodeType byteCodeType, Integer reg1, Integer longReg) {
            Integer id = destination->GetByteCodeCount();
            ByteCode* bc = destination->PushByteCode(runtime);
            bc->Init(runtime, static_cast<std::uint32_t>(ByteCodeType::NoOp));
            UpdateLong(runtime, id, byteCodeType, reg1, longReg);
            return id;
        }

        Integer Emit(Runtime* runtime, ByteCodeType byteCodeType, Integer reg1, Integer reg2) {
            return Emit(runtime, byteCodeType, reg1, reg2, Integer{0});
        }

        Integer Emit(Runtime* runtime, ByteCodeType byteCodeType, Integer reg1) {
            return Emit(runtime, byteCodeType, reg1, Integer{0});
        }

        Integer Emit(Runtime* runtime, ByteCodeType byteCodeType) {
            return Emit(runtime, byteCodeType, Integer{0});
        }

        Integer CurrentByteCodeLocation() {
            return destination->GetByteCodeCount();
        }

        void UpdateLong(Runtime* runtime, Integer byteCodeIndex, ByteCodeType type, Integer reg1, Integer longReg) {
            std::uint32_t result = 0;
            result |= static_cast<std::uint32_t>(type);
            result |= bits::ARG1_BITS & (static_cast<std::uint32_t>(reg1.Unwrap()) << bits::ARG1_SHIFT);
            result |= bits::LARGE_ARG_BITS & (static_cast<std::uint32_t>(longReg.Unwrap()) << bits::LARGE_ARG_SHIFT);
            ByteCode* bc = destination->ByteCodeAt(byteCodeIndex);
            bc->Init(runtime, result);
        }

        Integer LocalCount() {
            Scope* scope = this->scopes.At(Integer{this->scopes.Length().Unwrap() - 1});
            return Integer{scope->localsSize};
        }

        Integer StackTop() {
            Scope* scope = this->scopes.At(Integer{this->scopes.Length().Unwrap() - 1});
            if (scope->stackSize <= 0) {
                Panic("StackTop on empty stack");
            }
            return Integer{scope->localsSize + scope->stackSize - 1};
        }

        Integer RegisterCount() {
            Scope* scope = this->scopes.At(Integer{this->scopes.Length().Unwrap() - 1});
            return Integer{scope->localsSize + scope->stackSize};
        }

        void StackPop() {
            Scope* scope = this->scopes.At(Integer{this->scopes.Length().Unwrap() - 1});
            scope->stackSize--;
            if (scope->stackSize < 0) {
                // compiler bug
                Panic("Scope stack underflow");
            }
        }

        Integer NewBooleanConstant(Runtime* runtime, bool value) {
            Integer id = destination->GetConstantCount();
            destination->PushConstant(runtime)->SetBoolean(value);
            return id;
        }

        Integer NewNilConstant(Runtime* runtime) {
            Integer id = destination->GetConstantCount();
            destination->PushConstant(runtime)->SetNil();
            return id;
        }

        Integer NewStringConstant(Runtime* runtime, const char* message, std::int64_t length) {
            Integer id = destination->GetConstantCount();
            destination->PushConstant(runtime)->SetString(runtime->NewString(message, length));
            return id;
        }

        Integer NewIntegerConstant(Runtime* runtime, Integer value) {
            Integer id = destination->GetConstantCount();
            destination->PushConstant(runtime)->SetInteger(value);
            return id;
        }

        Integer NewDoubleConstant(Runtime* runtime, Double value) {
            Integer id = destination->GetConstantCount();
            destination->PushConstant(runtime)->SetDouble(value);
            return id;
        }

        Integer StackPush(Runtime* runtime) {
            Scope* scope = this->scopes.At(Integer{this->scopes.Length().Unwrap() - 1});
            scope->stackSize++;
            std::int64_t dest = StackTop().Unwrap();
            // registers are 8 bytes only, any other locals are unadressable
            if (dest >= REGISTER_COUNT) {
                Abort(runtime, "Too many values in the frame");
            }
            if (this->registerCount < RegisterCount().Unwrap()) {
                this->registerCount = RegisterCount().Unwrap();
            }
            return Integer{dest};
        }

        Integer StartDefineLocal(Runtime* runtime, Token token) {
            Scope* scope = this->scopes.At(Integer{this->scopes.Length().Unwrap() - 1});
            // TODO: locals size not needed b/c they are only ever allocated and 
            // freed in a strictly stack oriented manner
            if (scope->stackSize != 0) {
                // compiler bug
                Panic("Stack not cleared before local definition");
            }

            // ensure no duplicate
            std::int64_t localsCount = locals.Length().Unwrap();
            for (std::int64_t i = 0; i < localsCount; i++) {
                Variable* var = locals.At(Integer{i});
                if (0 == std::strncmp(var->token.source, token.source, var->token.length)) {
                    Abort(runtime, "Duplicate variable definition");
                }
            }

            // registers are 8 bytes only, any other locals are unadressable
            // TODO: refactor magic number
            Integer id = locals.Length();
            Variable* var = locals.Push(runtime);
            var->isDefined = false;
            var->token = token;
            return id;
        }

        void FinishDefineLocal(Runtime* runtime, Integer localId) {
            Scope* scope = this->scopes.At(Integer{this->scopes.Length().Unwrap() - 1});
            Variable* var = this->locals.At(localId);
            if (var->isDefined) {
                // compiler bug
                Panic("Duplicate define");
            }
            var->isDefined = true;
            scope->localsSize++;
            if (scope->stackSize != 0) {
                // compiler bug
                Panic("Stack not cleared after local definition");
            }
            if (RegisterCount().Unwrap() >= REGISTER_COUNT) {
                Abort(runtime, "Too many locals");
            }
            if (this->registerCount < RegisterCount().Unwrap()) {
                this->registerCount = RegisterCount().Unwrap();
            }
            if (scope->localsSize != this->locals.Length().Unwrap()) {
                Panic("After end of definition this->locals != scope->localsSize");
            }
            if (this->registerCount < scope->localsSize) {
                Panic("After end of definition registerCount < localsSize");
            }
        }
    };

    struct Tokenizer {

        struct Matcher {
            using Handle = std::int64_t(*)(Matcher* matcher, const char* head, std::int64_t length);

            TokenType type;
            Handle handle;
            union {
                const char* string;
            } data;
        };

        const char* string;
        std::int64_t length;
        std::int64_t index;
        Deque<Token, 2> tokenBuffer;
        Vector<Matcher> matchers;

        void Init(Runtime* runtime, String* source) {
            (void)(runtime);

            this->string = source->RawPointer();
            this->length = source->Length().Unwrap();
            this->index = 0;
            this->tokenBuffer = Deque<Token, 2>();
            this->matchers.Init(runtime);

            InitMatchers(runtime);
        }

        Token RawNext() {

            if (tokenBuffer.Length() > 0) {
                return tokenBuffer.Take();
            }

            if (this->index >= this->length) {
                return Token{TokenType::EndOfFile, "", 0};
            }

            const char* head = &this->string[this->index];
            std::int64_t length = this->length - this->index;

            std::int64_t matcherCount = this->matchers.Length().Unwrap();
            for (std::int64_t i = 0; i < matcherCount; i++) {
                Matcher* matcher = this->matchers.At(Integer{i});
                std::int64_t matchLength = matcher->handle(matcher, head, length);
                if (matchLength == 0) {
                    continue;
                }
                TokenType tokenType = matcher->type;
                this->index += matchLength;
                return Token{tokenType, head, matchLength};
            }

            Panic("No tokenizer match");
            throw nullptr;
        }

        void DeInit(Runtime* runtime) {
            this->matchers.DeInit(runtime);
        }

        void PutBack(Token* token) {
            tokenBuffer.Put(token);
        }

        Token Expect(Runtime* runtime, TokenType type) {
            Token result = Next();
            if (result.type != type) {
                constexpr static std::size_t BUFFER_SIZE = 100;
                char buffer[BUFFER_SIZE];
                std::snprintf(buffer, BUFFER_SIZE, "Expected a %s", TokenTypeToString(type));
                buffer[BUFFER_SIZE - 1] = '\0';
                Abort(runtime, buffer);
            }
            return result;
        }

        Token Next() {
            SkipWhiteSpace();
            return RawNext();
        }

        void SkipWhiteSpace() {
            while (true) {
                Token curr = RawNext();
                bool eof = curr.type == TokenType::EndOfFile;
                if (eof) {
                    return;
                }
                if (curr.type == TokenType::WhiteSpace) {
                    // don't put it back and go to next token
                    continue;
                }
                // otherwise put it back as it's not a whitespace and not eof
                PutBack(&curr);
                break;
            }
        }

        bool AtEof() {
            SkipWhiteSpace();
            Token t = RawNext();
            bool eof = t.type == TokenType::EndOfFile;
            PutBack(&t);
            return eof;
        }

        static std::int64_t literalMatcher(Matcher* matcher, const char* source, std::int64_t length) {
            const char* data = matcher->data.string;
            std::int64_t i = 0;
            while (i < length && data[i] != '\0') {
                if (data[i] != source[i]) {
                    return 0;
                }
                i++;
            }
            if (data[i] == '\0') {
                return std::strlen(data);
            } else {
                return 0;
            }
            return 0;
        }

        void InitMatchers(Runtime* runtime) {
            
            using CharFn = int(*)(int);

            Matcher::Handle literalNeedingSeparatorMatcher = [](Matcher* matcher, const char* source, std::int64_t length) -> std::int64_t {
                std::int64_t matchLength = literalMatcher(matcher, source, length);
                if (matchLength == 0) {
                    return 0;
                }
                if (matchLength == length) {
                    return matchLength;
                }
                char next = source[matchLength];
                if (std::isalnum(next)) {
                    // not separated
                    return 0;
                }
                return matchLength;
            };

            Matcher::Handle unknownMatcher = [](Matcher* matcher, const char* source, std::int64_t length) -> std::int64_t {
                (void)(matcher);
                (void)(source);
                (void)(length);
                return 1;
            };

            Matcher::Handle identifierMatcher = [](Matcher* matcher, const char* source, std::int64_t length) -> std::int64_t {
                (void)(matcher);
                std::int64_t i = 0;
                for (; i < length; i++) {
                    CharFn checker = (i == 0) ? std::isalpha : std::isalnum;
                    char c = source[i];
                    if (!checker(c)) {
                        break;
                    }
                }
                return i;
            };

            Matcher::Handle doubleMatcher = [](Matcher* matcher, const char* source, std::int64_t length) -> std::int64_t {
                (void)(matcher);
                std::int64_t i = 0;
                bool foundPeriod = false;
                for (; i < length; i++) {
                    char c = source[i];
                    if (c == '.' && !foundPeriod) {
                        foundPeriod = true;
                        continue;
                    }
                    if (!std::isdigit(c)) {
                        break;
                    }
                }
                if (!foundPeriod) {
                    return 0;
                }
                return i;
            };

            Matcher::Handle integerMatcher = [](Matcher* matcher, const char* source, std::int64_t length) -> std::int64_t {
                (void)(matcher);
                std::int64_t i = 0;
                for (; i < length; i++) {
                    char c = source[i];
                    if (!std::isdigit(c)) {
                        break;
                    }
                }
                return i;
            };

            Matcher::Handle stringMatcher = [](Matcher* matcher, const char* source, std::int64_t length) -> std::int64_t {
                (void)(matcher);
                std::int64_t i = 0;
                bool foundClosing = false;
                for (; i < length; i++) {
                    char c = source[i];
                    if (i == 0) {
                        if (c != '\"') {
                            return 0;
                        }
                    } else {
                        if (c == '\"') {
                            i++;
                            foundClosing = true;
                            break;
                        }
                    }
                }
                if (!foundClosing) {
                    return 0;
                }
                return i;
            };
            
            Matcher::Handle commentMatcher = [](Matcher* matcher, const char* source, std::int64_t length) -> std::int64_t {
                (void)(matcher);
                std::int64_t i = 0;
                for (; i < length; i++) {
                    char c = source[i];
                    if (i == 0) {
                        if (c != '/') {
                            return 0;
                        }
                    } else if (i == 1) {
                        if (c != '*') {
                            return 0;
                        }
                    } else if (c == '*') {
                        if (i + 1 > length) {
                            return 0;
                        }
                        char next = source[i + 1];
                        if (next == '/') {
                            return i + 2;
                        }
                    }
                }
                return 0;
            };

            auto literal = [&](TokenType type, const char* value) {
                Matcher* matcher = this->matchers.Push(runtime);
                matcher->type = type;
                matcher->data.string = value;
                matcher->handle = literalMatcher;
            };

            auto literalNeedingSeparator = [&](TokenType type, const char* value) {
                Matcher* matcher = this->matchers.Push(runtime);
                matcher->type = type;
                matcher->data.string = value;
                matcher->handle = literalNeedingSeparatorMatcher;
            };

            auto matcherFromHandle = [&](TokenType type, Matcher::Handle handle) {
                Matcher* matcher = this->matchers.Push(runtime);
                matcher->type = type;
                matcher->data.string = nullptr;
                matcher->handle = handle;
            };

            literalNeedingSeparator(TokenType::Global, "global");
            literalNeedingSeparator(TokenType::Var, "var");
            literalNeedingSeparator(TokenType::If, "if");
            literalNeedingSeparator(TokenType::Else, "else");
            literalNeedingSeparator(TokenType::While, "while");
            literalNeedingSeparator(TokenType::Return, "return");
            literalNeedingSeparator(TokenType::Boolean, "true");
            literalNeedingSeparator(TokenType::Boolean, "false");
            literalNeedingSeparator(TokenType::Nil, "nil");
            literalNeedingSeparator(TokenType::Fn, "fn");
            literal(TokenType::Assign, "=");
            literal(TokenType::LeftParen, "(");
            literal(TokenType::RightParen, ")");
            literal(TokenType::Period, ".");
            literal(TokenType::LeftCurly, "{");
            literal(TokenType::RightCurly, "}");
            literal(TokenType::SemiColon, ";");
            literal(TokenType::WhiteSpace, " ");
            literal(TokenType::WhiteSpace, "\t");
            literal(TokenType::WhiteSpace, "\r");
            literal(TokenType::WhiteSpace, "\n");
            literal(TokenType::WhiteSpace, "\r\n");
            matcherFromHandle(TokenType::Identifier, identifierMatcher);
            matcherFromHandle(TokenType::Double, doubleMatcher);
            matcherFromHandle(TokenType::Integer, integerMatcher);
            matcherFromHandle(TokenType::String, stringMatcher);
            matcherFromHandle(TokenType::WhiteSpace, commentMatcher);
            matcherFromHandle(TokenType::Unknown, unknownMatcher);
        }
    };

    Tokenizer tokenizer;
    Vector<Context> contexts;

    Compiler(Runtime* runtime, String* source, Function* destination) {
        tokenizer.Init(runtime, source);
        contexts.Init(runtime);
        PushContext(runtime)->Init(runtime, destination);
    }

    void DeInit(Runtime* runtime) {
        tokenizer.DeInit(runtime);
        while (contexts.Length().Unwrap() > 0) {
            PopContext(runtime);
        }
        contexts.DeInit(runtime);
    }

    void PopContext(Runtime* runtime) {
        Context* toFree = CurrentContext();
        toFree->DeInit(runtime);
        contexts.Pop();
    }

    Context* PushContext(Runtime* runtime) {
        return this->contexts.Push(runtime);
    }

    void Compile(Runtime* runtime) {
        while (!tokenizer.AtEof()) {
            CompileStatement(runtime);
        }
    }

    void CompileStatement(Runtime* runtime) {
        Token current = tokenizer.Next();
        switch (current.type) {
            case TokenType::Var: {
                tokenizer.PutBack(&current);
                CompileVarStatement(runtime);
                break;
            }
            case TokenType::Global: {
                tokenizer.PutBack(&current);
                CompileGlobalStatement(runtime);
                break;
            }
            case TokenType::While: {
                tokenizer.PutBack(&current);
                CompileWhileStatement(runtime);
                break;
            }
            case TokenType::If: {
                tokenizer.PutBack(&current);
                CompileIfStatement(runtime);
                break;
            }
            case TokenType::Return: {
                tokenizer.PutBack(&current);
                CompileReturnStatement(runtime);
                break;
            }
            default: {
                tokenizer.PutBack(&current);
                CompileExpressionStatement(runtime);
                break;
            }
        }
    }

    void CompileReturnStatement(Runtime* runtime) {
        tokenizer.Expect(runtime, TokenType::Return);
        CompileExpression(runtime);
        Integer top = CurrentContext()->StackTop();
        CurrentContext()->StackPop();
        CurrentContext()->Emit(runtime, ByteCodeType::Return, top);
        tokenizer.Expect(runtime, TokenType::SemiColon);
    }

    void CompileWhileStatement(Runtime* runtime) {
        Panic("TODO");
        (void)(runtime);
    }

    void CompileIfStatement(Runtime* runtime) {
        tokenizer.Expect(runtime, TokenType::If);
        CompileExpression(runtime);
        Integer top = CurrentContext()->StackTop();
        CurrentContext()->StackPop();
        Integer jumpLocation = CurrentContext()->EmitLong(runtime, ByteCodeType::JumpIfFalse, top, Integer{0});
        tokenizer.Expect(runtime, TokenType::LeftCurly);

        // TODO: scope management
        while (true) {
            Token curr = tokenizer.Next();
            if (curr.type == TokenType::RightCurly) {
                tokenizer.PutBack(&curr);
                break;
            }
            tokenizer.PutBack(&curr);
            CompileStatement(runtime);
        }

        tokenizer.Expect(runtime, TokenType::RightCurly);

        Integer endOfIfTrue = CurrentContext()->CurrentByteCodeLocation();
        CurrentContext()->UpdateLong(runtime, jumpLocation, ByteCodeType::JumpIfFalse, top, endOfIfTrue);

        // TODO: else if
        // TODO: block as a sub component of statement?
    }

    void CompileExpressionStatement(Runtime* runtime) {
        CompileExpression(runtime);
        tokenizer.Expect(runtime, TokenType::SemiColon);
        CurrentContext()->StackPop();
    }

    void CompileGlobalStatement(Runtime* runtime) {
        tokenizer.Expect(runtime, TokenType::Global);
        Token identifier = tokenizer.Expect(runtime, TokenType::Identifier);
        Integer constantNumber = CurrentContext()->NewStringConstant(runtime, identifier.source, identifier.length);
        tokenizer.Expect(runtime, TokenType::Assign);
        CompileExpression(runtime);
        // there should be something at the top of the stack that 
        // we will use as the value of the expression
        Integer globalValue = CurrentContext()->StackTop();
        Integer registerDest = CurrentContext()->StackPush(runtime);
        CurrentContext()->EmitLong(runtime, ByteCodeType::LoadConstant, registerDest, constantNumber);
        Integer globalKey = CurrentContext()->StackTop();
        CurrentContext()->StackPop(); // key
        CurrentContext()->StackPop(); // value
        CurrentContext()->Emit(runtime, ByteCodeType::StoreGlobal, globalKey, globalValue);
        tokenizer.Expect(runtime, TokenType::SemiColon);
    }

    void CompileVarStatement(Runtime* runtime) {
        tokenizer.Expect(runtime, TokenType::Var);
        Token identifier = tokenizer.Expect(runtime, TokenType::Identifier);
        Integer localNumber = CurrentContext()->StartDefineLocal(runtime, identifier);
        tokenizer.Expect(runtime, TokenType::Assign);
        CompileExpression(runtime);
        Integer top = CurrentContext()->StackTop();
        CurrentContext()->StackPop();
        CurrentContext()->FinishDefineLocal(runtime, localNumber);
        if (top.Unwrap() != localNumber.Unwrap()) {
            Panic("Compiler bug: var not defined in correct position");
        }
        tokenizer.Expect(runtime, TokenType::SemiColon);
    }

    void CompileExpression(Runtime* runtime) {
        Token current = tokenizer.Next();
        tokenizer.PutBack(&current);
        switch (current.type) {
            case TokenType::Nil: {
                CompileNil(runtime);
                return;
            }
            case TokenType::String: {
                CompileString(runtime);
                return;
            }
            case TokenType::Integer: {
                CompileInteger(runtime);
                return;
            }
            case TokenType::Double: {
                CompileDouble(runtime);
                return;
            }
            case TokenType::Boolean: {
                CompileBoolean(runtime);
                return;
            }
            case TokenType::Identifier: {
                if (tokenizer.AtEof()) {
                    CompileIdentifier(runtime);
                } else {
                    Token next = tokenizer.Next();
                    if (next.type == TokenType::LeftParen) {
                        tokenizer.PutBack(&next);
                        CompileInvoke(runtime);
                    } else {
                        CompileIdentifier(runtime);
                    }
                }
                return;
            }
            case TokenType::LeftParen: {
                // regular parens which have no ompact on expression
                // simply unwrap them
                tokenizer.Expect(runtime, TokenType::LeftParen);
                CompileExpression(runtime);
                tokenizer.Expect(runtime, TokenType::RightParen);
                return;
            }
            default: {
                Abort(runtime, "Expected an expression");
            }
        }
    }

    void CompileNil(Runtime* runtime) {
        tokenizer.Expect(runtime, TokenType::Nil);
        Integer constantNumber = CurrentContext()->NewNilConstant(runtime);
        Integer registerDest = CurrentContext()->StackPush(runtime);
        CurrentContext()->EmitLong(runtime, ByteCodeType::LoadConstant, registerDest, constantNumber);
    }

    void CompileBoolean(Runtime* runtime) {
        Token token = tokenizer.Expect(runtime, TokenType::Boolean);
        bool isTrue = 0 == std::strncmp("true", token.source, token.length);
        Integer constantNumber = CurrentContext()->NewBooleanConstant(runtime, isTrue);
        Integer registerDest = CurrentContext()->StackPush(runtime);
        CurrentContext()->EmitLong(runtime, ByteCodeType::LoadConstant, registerDest, constantNumber);
    }

    void CompileIdentifier(Runtime* runtime) {
        Panic("TODO");
        (void)(runtime);
    }

    void CompileInvoke(Runtime* runtime) {
        Panic("TODO");
        (void)(runtime);
    }

    void CompileInteger(Runtime* runtime) {
        Token token = tokenizer.Expect(runtime, TokenType::Integer);
        char* end = const_cast<char*>(&token.source[token.length]);
        std::int64_t value = std::strtoll(token.source, &end, 10);
        if (&token.source[token.length] != end) {
            Abort(runtime, "Invalid integer");
        }
        Integer constantNumber = CurrentContext()->NewIntegerConstant(runtime, Integer{value});
        Integer registerDest = CurrentContext()->StackPush(runtime);
        CurrentContext()->EmitLong(runtime, ByteCodeType::LoadConstant, registerDest, constantNumber);
    }

    void CompileDouble(Runtime* runtime) {
        Token token = tokenizer.Expect(runtime, TokenType::Double);
        char* end = const_cast<char*>(&token.source[token.length]);
        double doubleValue = std::strtod(token.source, &end);
        if (errno != 0) {
            Abort(runtime, "Error reading real");
        }
        if (&token.source[token.length] != end) {
            Abort(runtime, "Invalid real");
        }
        if (std::isnan(doubleValue)) {
            Abort(runtime, "Invalid real");
        }
        Integer constantNumber = CurrentContext()->NewDoubleConstant(runtime, Double{doubleValue});
        Integer registerDest = CurrentContext()->StackPush(runtime);
        CurrentContext()->EmitLong(runtime, ByteCodeType::LoadConstant, registerDest, constantNumber);
    }

    void CompileString(Runtime* runtime) {
        Token token = tokenizer.Expect(runtime, TokenType::String);

        const char* afterQuote = &token.source[1];
        std::int64_t lengthWithoutQuotes = token.length - 2;

        if (lengthWithoutQuotes < 0) {
            Panic("CompileString length < 0");
        }

        Integer constantNumber = CurrentContext()->NewStringConstant(runtime, afterQuote, lengthWithoutQuotes);
        Integer registerDest = CurrentContext()->StackPush(runtime);
        CurrentContext()->EmitLong(runtime, ByteCodeType::LoadConstant, registerDest, constantNumber);
    }

    Context* CurrentContext() {
        return this->contexts.At(Integer{this->contexts.Length().Unwrap() - 1});
    }

};

void Compile(Runtime* rt) {
    rt->Local(Integer{0})->SetFunction(rt->NewFunction());
    Function* dest = rt->Local(Integer{0})->GetFunction(rt);
    String* source = rt->Local(Integer{1})->GetString(rt);

    Compiler compiler{rt, source, dest};

    compiler.Compile(rt);

    compiler.DeInit(rt);
}

} // namespace compiler

} // namespace native

} // namespace espresso
