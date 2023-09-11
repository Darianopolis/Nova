#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Ref.hpp>

namespace nova
{
    /*

fun main(): void {
    ref u: Uniforms = pc.uniforms;
    ref v: Vertex v = pc.vertices[gl_VertexIndex];
    color = v.color;
    gl_Position = vec4(v.position + u.offset, 1);
}

ref box: ImRoundRect = pc.rects[inInstanceID];

var absPos: vec2 = abs(inTex);
var cornerFocus: vec2 = box.halfExtent - vec2(box.cornerRadius);

var sampled: vec4 = box.texTint.a > 0
    ? box.texTint * texture(Sampler2D(nonuniformEXT(box.texIndex), 0),
        (inTex / box.halfExtent) * box.texHalfExtent + box.texCenterPos)
    : vec4(0);
var centerColor: vec4 = vec4(
    sampled.rgb * sampled.a + box.centerColor.rgb * (1 - sampled.a),
    sampled.a + box.centerColor.a * (1 - sampled.a));

if (absPos.x > cornerFocus.x && absPos.y > cornerFocus.y) {
    var dist: float = length(absPos - cornerFocus);
    if (dist > box.cornerRadius + 0.5) {
        discard;
    }

    outColor = (dist > box.cornerRadius - box.borderWidth + 0.5)
        ? vec4(box.borderColor.rgb, box.borderColor.a * (1 - max(0, dist - (box.cornerRadius - 0.5))))
        : mix(centerColor, box.borderColor, max(0, dist - (box.cornerRadius - box.borderWidth - 0.5)));
} else {
    outColor = (absPos.x > box.halfExtent.x - box.borderWidth || absPos.y > box.halfExtent.y - box.borderWidth)
        ? box.borderColor
        : centerColor;
}

    */

    enum class TokenType
    {
        Invalid,

        LeftParen, RightParen,
        LeftBrace, RightBrace,
        LeftBracket, RightBracket,

        Comma, Dot, Colon,
        Plus, Minus, Star,
        Caret, Semicolon,

        Ampersand, AmpersandAmpersand,
        Pipe, PipePipe,
        Bang, BangEqual,
        Equal, EqualEqual,
        Greater, GreaterEqual,
        Less, LessEqual,

        Slash,

        Identifier, String, Number,

        And, Else, False, For, If, Or, Nil,
        Return, True, While, Void, Fun, Var, Ref,

        Eof
    };

    std::string_view ToString(TokenType type);

    struct Token
    {
        TokenType          type = TokenType::Invalid;
        std::string_view lexeme;
        i32                line = 0;
    };

    struct Compiler
    {
        bool hadError = false;

        void Error(i32 line, std::string_view message);
        void Error(const Token& token, std::string_view message);
        void Report(i32 line, std::string_view where, std::string_view message);
    };

    struct Scanner
    {
        Compiler* compiler;

        std::string_view   source;
        std::vector<Token> tokens;

        i32   start = 0;
        i32 current = 0;
        i32    line = 1;

        static bool IsAlpha(char c);
        static bool IsDigit(char c);
        static bool IsAlphaNumeric(char c);

        bool IsAtEnd();
        void ScanTokens();
        void ScanToken();
        void AddToken(TokenType type);
        char Advance();
        bool Match(char expected);
        char Peek();
        char PeekNext();
        void String();
        void Number();
        void Identifier();
    };

    enum class AstNodeType
    {
        Invalid,

        Function, VarDecl, If, Return, While, Block, Variable,
        Assign, Get, Set, Logical, Binary, Unary, Call, Literal
    };

    struct AstNode { AstNodeType type; };
    struct AstFunction : AstNode { Token name; std::optional<Token> type; std::vector<AstNode*> parameters; AstNode* body; };
    struct AstVarDecl  : AstNode { Token name; std::optional<Token> type; AstNode* initializer; };
    struct AstIf       : AstNode { AstNode* cond; AstNode* thenBranch; AstNode* elseBranch; };
    struct AstReturn   : AstNode { Token keyword; AstNode* value; };
    struct AstWhile    : AstNode { AstNode* condition; AstNode* body; };
    struct AstBlock    : AstNode { std::vector<AstNode*> statements; };
    struct AstVariable : AstNode { Token name; };
    struct AstAssign   : AstNode { Token name; AstNode* value; };
    struct AstGet      : AstNode { AstNode* object; Token name; };
    struct AstSet      : AstNode { AstNode* object; Token name; AstNode* value; };
    struct AstLogical  : AstNode { Token op; AstNode* left; AstNode* right; };
    struct AstBinary   : AstNode { Token op; AstNode* left; AstNode* right; };
    struct AstUnary    : AstNode { Token op; AstNode* right; };
    struct AstCall     : AstNode { AstNode* callee; Token paren; std::vector<AstNode*> arguments; };
    struct AstLiteral  : AstNode { Token token; };

    template<typename Fn>
    void VisitNode(Fn&& fn, AstNode* expr)
    {
        if (!expr) return;
        switch (expr->type)
        {
        break;case AstNodeType::Function: fn(static_cast<AstFunction*>(expr));
        break;case AstNodeType::VarDecl:  fn(static_cast<AstVarDecl*>(expr));
        break;case AstNodeType::If:       fn(static_cast<AstIf*>(expr));
        break;case AstNodeType::Return:   fn(static_cast<AstReturn*>(expr));
        break;case AstNodeType::While:    fn(static_cast<AstWhile*>(expr));
        break;case AstNodeType::Block:    fn(static_cast<AstBlock*>(expr));
        break;case AstNodeType::Variable: fn(static_cast<AstVariable*>(expr));
        break;case AstNodeType::Assign:   fn(static_cast<AstAssign*>(expr));
        break;case AstNodeType::Get:      fn(static_cast<AstGet*>(expr));
        break;case AstNodeType::Set:      fn(static_cast<AstSet*>(expr));
        break;case AstNodeType::Logical:  fn(static_cast<AstLogical*>(expr));
        break;case AstNodeType::Binary:   fn(static_cast<AstBinary*>(expr));
        break;case AstNodeType::Unary:    fn(static_cast<AstUnary*>(expr));
        break;case AstNodeType::Call:     fn(static_cast<AstCall*>(expr));
        break;case AstNodeType::Literal:  fn(static_cast<AstLiteral*>(expr));
        }
    }

    struct ParseError : std::exception {};

    struct Parser
    {
        Scanner* scanner;
        i32 current = 0;

        std::vector<AstNode*> nodes;

        bool Match(Span<TokenType> types);
        bool Check(TokenType type);
        Token& Advance();
        bool IsAtEnd();
        Token& Peek();
        Token& Previous();
        Token& Consume(TokenType type, std::string_view message);
        ParseError Error(const Token& token, std::string_view message);
        void Synchronize();

        void Parse();

        AstNode* Declaration();
        AstNode* StructDeclaration();
        AstNode* Function();
        AstNode* VarDeclaration();
        AstNode* Statement();
        AstNode* ForStatement();
        AstNode* IfStatement();
        AstNode* ReturnStatement();
        AstNode* WhileStatement();
        AstNode* Block();
        AstNode* ExpressionStatement();
        AstNode* Expression();
        AstNode* Assignment();
        AstNode* Or();
        AstNode* And();
        AstNode* Equality();
        AstNode* Comparison();
        AstNode* Term();
        AstNode* Factor();
        AstNode* Unary();
        AstNode* Call();
        AstNode* FinishCall(AstNode* callee);
        AstNode* Primary();
    };
}