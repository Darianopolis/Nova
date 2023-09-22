#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Ref.hpp>

namespace nova
{
    /*
fn main()\
{\
    let u: Uniforms = pc.uniforms;\
    let v: Vertex = pc.vertices[gl_VertexIndex];\
    color = v.color;\
    gl_Position = vec4(v.position + u.offset, 1);\
}

fn main()\
{\
    let u = pc.uniforms;\
    let v = pc.vertices[gl_VertexIndex];\
    color = v.color;\
    gl_Position = vec4(v.position + u.offset, 1);\
}

fun main()\
{\
    let box = pc.rects[inInstanceID];\
\
    let absPos: vec2 = abs(inTex);\
    let cornerFocus: vec2 = box.halfExtent - vec2(box.cornerRadius);\
\
    let sampled: vec4 = box.texTint.a > 0\
        ? box.texTint * texture(Sampler2D(nonuniformEXT(box.texIndex), 0),\
            (inTex / box.halfExtent) * box.texHalfExtent + box.texCenterPos)\
        : vec4(0);\
    let centerColor: vec4 = vec4(\
        sampled.rgb * sampled.a + box.centerColor.rgb * (1 - sampled.a),\
        sampled.a + box.centerColor.a * (1 - sampled.a));\
\
    if (absPos.x > cornerFocus.x && absPos.y > cornerFocus.y) {\
        let dist: float = length(absPos - cornerFocus);\
        if (dist > box.cornerRadius + 0.5) {\
            discard;\
        }\
\
        outColor = (dist > box.cornerRadius - box.borderWidth + 0.5)\
            ? vec4(box.borderColor.rgb, box.borderColor.a * (1 - max(0, dist - (box.cornerRadius - 0.5))))\
            : mix(centerColor, box.borderColor, max(0, dist - (box.cornerRadius - box.borderWidth - 0.5)));\
    } else {\
        outColor = (absPos.x > box.halfExtent.x - box.borderWidth || absPos.y > box.halfExtent.y - box.borderWidth)\
            ? box.borderColor\
            : centerColor;\
    }\
}

    */

// -----------------------------------------------------------------------------
//                                  Tokens
// -----------------------------------------------------------------------------

    enum class TokenType
    {
        None,

        LeftParen, RightParen,
        LeftBrace, RightBrace,
        LeftBracket, RightBracket,

        Comma, Dot, Colon, QuestionMark,
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

        Else, False, For, If, Nil,
        Return, True, While, Void, Fun, Let,

        Eof
    };

    std::string_view ToString(TokenType type);

    struct Token
    {
        TokenType          type;
        i32                line;
        std::string_view lexeme;

        Token(TokenType _type, i32 _line, std::string_view _lexeme)
            : type(_type)
            , line(_line)
            , lexeme(_lexeme)
        {}
    };

    struct Compiler;

// -----------------------------------------------------------------------------
//                                 Scanner
// -----------------------------------------------------------------------------

    struct Scanner
    {
        Compiler* compiler = nullptr;

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

// -----------------------------------------------------------------------------
//                           Abstract Syntax Tree
// -----------------------------------------------------------------------------

    struct AstNode;

    struct AstNodeList
    {
        AstNode* head = nullptr;
    };

    struct AstNodeListBuilder
    {
        AstNode* head = nullptr;
        AstNode* tail = nullptr;

        AstNodeListBuilder() = default;
        AstNodeListBuilder(AstNodeList list);

        void Append(AstNode* toAppend);
        AstNodeList ToList() const;
    };

    enum class AstNodeType
    {
        Function, VarDecl, If, Return, While, Block, Variable,
        Assign, Get, Logical, Binary, Unary, Call, Literal, CondExpr
    };

    struct AstNode { AstNodeType nodeType; AstNode* next = nullptr; };

    struct AstFunction : AstNode { Token* name; Token* type; AstNodeList parameters; AstNode* body; };
    struct AstVarDecl  : AstNode { Token* name; Token* type; AstNode* initializer; };
    struct AstIf       : AstNode { AstNode* cond; AstNode* thenBranch; AstNode* elseBranch; };
    struct AstReturn   : AstNode { Token* keyword; AstNode* value; };
    struct AstWhile    : AstNode { AstNode* condition; AstNode* body; };
    struct AstBlock    : AstNode { AstNodeList statements; };
    struct AstVariable : AstNode { Token* name; };
    struct AstAssign   : AstNode { AstNode* variable; AstNode* value; };
    struct AstGet      : AstNode { AstNode* object; Token* name; };
    struct AstLogical  : AstNode { Token* op; AstNode* left; AstNode* right; };
    struct AstBinary   : AstNode { Token* op; AstNode* left; AstNode* right; };
    struct AstUnary    : AstNode { Token* op; AstNode* right; };
    struct AstCall     : AstNode { AstNode* callee; Token* paren; AstNodeList arguments; };
    struct AstLiteral  : AstNode { Token* token; };
    struct AstCondExpr : AstNode { AstNode* cond; AstNode* thenExpr; AstNode* elseExpr; };

    template<typename Ret = void, typename Fn>
    Ret VisitNode(Fn&& fn, AstNode* expr)
    {
        if (!expr) return Ret();
        switch (expr->nodeType)
        {
        break;case AstNodeType::Function: return fn(static_cast<AstFunction*>(expr));
        break;case AstNodeType::VarDecl:  return fn(static_cast<AstVarDecl*>(expr));
        break;case AstNodeType::If:       return fn(static_cast<AstIf*>(expr));
        break;case AstNodeType::Return:   return fn(static_cast<AstReturn*>(expr));
        break;case AstNodeType::While:    return fn(static_cast<AstWhile*>(expr));
        break;case AstNodeType::Block:    return fn(static_cast<AstBlock*>(expr));
        break;case AstNodeType::Variable: return fn(static_cast<AstVariable*>(expr));
        break;case AstNodeType::Assign:   return fn(static_cast<AstAssign*>(expr));
        break;case AstNodeType::Get:      return fn(static_cast<AstGet*>(expr));
        break;case AstNodeType::Logical:  return fn(static_cast<AstLogical*>(expr));
        break;case AstNodeType::Binary:   return fn(static_cast<AstBinary*>(expr));
        break;case AstNodeType::Unary:    return fn(static_cast<AstUnary*>(expr));
        break;case AstNodeType::Call:     return fn(static_cast<AstCall*>(expr));
        break;case AstNodeType::Literal:  return fn(static_cast<AstLiteral*>(expr));
        break;case AstNodeType::CondExpr: return fn(static_cast<AstCondExpr*>(expr));
        }

        NOVA_THROW("Invalid node type");
    }

// -----------------------------------------------------------------------------
//                                  Parser
// -----------------------------------------------------------------------------

    struct ParseError : std::exception {};

    struct Parser
    {
        Scanner*  scanner = nullptr;
        i32       current = 0;
        AstNodeList nodes;

        bool Match(Span<TokenType> types);
        bool Check(TokenType type);
        Token* Advance();
        bool IsAtEnd();
        Token* Peek();
        Token* Previous();
        Token* Consume(TokenType type, std::string_view message);
        ParseError Error(Token* token, std::string_view message);
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
        AstNode* FinishCall(AstNode* callee, TokenType closingToken);
        AstNode* Primary();
    };

// -----------------------------------------------------------------------------
//                                 Compiler
// -----------------------------------------------------------------------------

    struct Type;

    struct Struct
    {
        HashMap<std::string_view, Type*> members;
    };

    struct Type
    {
        std::string_view name = "";
        bool       registered = false;
        Struct*     structure = nullptr;
    };

// -----------------------------------------------------------------------------
//                                 Compiler
// -----------------------------------------------------------------------------

    struct Compiler
    {
        bool hadError = false;

        void Error(i32 line, std::string_view message);
        void Error(Token* token, std::string_view message);
        void Report(i32 line, std::string_view where, std::string_view message);
    };
}