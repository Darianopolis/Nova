#include "nova_Lang.hpp"

namespace nova
{
    bool Parser::Match(Span<TokenType> types)
    {
        for (auto type : types) {
            if (Check(type)) {
                Advance();
                return true;
            }
        }

        return false;
    }

    bool Parser::Check(TokenType type)
    {
        if (IsAtEnd()) return false;
        return Peek().type == type;
    }

    Token& Parser::Advance()
    {
        if (!IsAtEnd()) current++;
        return Previous();
    }

    bool Parser::IsAtEnd()
    {
        return Peek().type == TokenType::Eof;
    }

    Token& Parser::Peek()
    {
        return scanner->tokens[current];
    }

    Token& Parser::Previous()
    {
        return scanner->tokens[current - 1];
    }

    Token& Parser::Consume(TokenType type, std::string_view message)
    {
        if (Check(type)) return Advance();

        throw Error(Peek(), message);
    }

    ParseError Parser::Error(const Token& token, std::string_view message)
    {
        scanner->compiler->Error(token, message);
        return ParseError();
    }

    void Parser::Synchronize()
    {
        Advance();

        while (!IsAtEnd()) {
            if (Previous().type == TokenType::Semicolon) return;

            switch (Peek().type)
            {
            break;case TokenType::If:
                  case TokenType::Return:
                return;
            }

            Advance();
        }
    }

    void Parser::Parse()
    {
        try {
            while (!IsAtEnd()) {
                nodes.emplace_back(Declaration());
            }
        } catch (ParseError) {
            NOVA_LOG("Parse completed with errors");
            nodes.clear();
        }
    }

// -----------------------------------------------------------------------------

    AstNode* Parser::Declaration()
    {
        try {
            if (Match({TokenType::Fun})) return Function();
            if (Match({TokenType::Var, TokenType::Ref})) return VarDeclaration();

            return Statement();
        } catch (ParseError) {
            Synchronize();
            return {};
        }
    }

    AstNode* Parser::StructDeclaration()
    {
        scanner->compiler->Error(Previous(), "struct definitions not implemented");
        throw ParseError();
    }

    AstNode* Parser::Function()
    {
        auto& name = Consume(TokenType::Identifier, "Expect function name.");

        Consume(TokenType::LeftParen, "Expect '(' after function name.");
        auto parameters = std::vector<AstNode*>();
        if (!Check(TokenType::RightParen)) {
            do {
                parameters.emplace_back(VarDeclaration());
            } while (Match({TokenType::Comma}));
        }

        Consume(TokenType::RightParen, "Expect ')' after parameters");

        std::optional<Token> type;
        if (Match({TokenType::Colon}) && !Match({TokenType::Void})) {
            type = Consume(TokenType::Identifier, "Expect identifier after explicit function return type declaration.");
        }

        Consume(TokenType::LeftBrace, "Expect '{' before function body.");
        auto body = Block();

        return new AstFunction{AstNodeType::Function, name, type, std::move(parameters), body};
    }

    AstNode* Parser::VarDeclaration()
    {
        auto& name = Consume(TokenType::Identifier, "Expect variable name.");

        std::optional<Token> type;
        if (Match({TokenType::Colon})) {
            type = Consume(TokenType::Identifier, "Expect identifier after explicit type declaration.");
        }

        auto initializer = Match({TokenType::Equal})
            ? Expression()
            : nullptr;

        return new AstVarDecl{AstNodeType::VarDecl, name, type, initializer};
    }

    AstNode* Parser::Statement()
    {
        if (Match({TokenType::For})) return ForStatement();
        if (Match({TokenType::If})) return IfStatement();
        if (Match({TokenType::Return})) return ReturnStatement();
        if (Match({TokenType::While})) return WhileStatement();
        if (Match({TokenType::LeftBrace})) return Block();

        return ExpressionStatement();
    }

    AstNode* Parser::ForStatement()
    {
        scanner->compiler->Error(Previous(), "for loops not implemented.");
        throw ParseError();
    }

    AstNode* Parser::IfStatement()
    {
        Consume(TokenType::LeftParen, "Expect '(' after 'if'.");
        auto condition = Expression();
        Consume(TokenType::RightParen, "Expect ')' after if condition.");

        auto thenBranch = Statement();
        auto elseBranch = Match({TokenType::Else})
            ? Statement()
            : nullptr;

        return new AstIf{AstNodeType::If, condition, thenBranch, elseBranch};
    }

    AstNode* Parser::ReturnStatement()
    {
        auto keyword = Previous();
        auto value = Check(TokenType::Semicolon) ? nullptr : Expression();
        Consume(TokenType::Semicolon, "Expect ';' after return statement.");
        return new AstReturn{AstNodeType::Return, keyword, value};
    }

    AstNode* Parser::WhileStatement()
    {
        Consume(TokenType::LeftParen, "Expect '(' after 'while'.");
        auto condition = Expression();
        Consume(TokenType::RightParen, "Expect ')' after condition.");
        auto body = Statement();

        return new AstWhile{AstNodeType::While, condition, body};
    }

    AstNode* Parser::Block()
    {
        auto statements = std::vector<AstNode*>();

        while (!Check(TokenType::RightBrace) && !IsAtEnd()) {
            statements.push_back(Declaration());
        }

        Consume(TokenType::RightBrace, "Expect '}' after block.");
        return new AstBlock{AstNodeType::Block, std::move(statements)};
    }

    AstNode* Parser::ExpressionStatement()
    {
        auto expr = Expression();
        Consume(TokenType::Semicolon, "Expect ';' after expression.");
        return expr;
    }

    AstNode* Parser::Expression()
    {
        return Assignment();
    }

    AstNode* Parser::Assignment()
    {
        auto expr = Or();

        if (Match({TokenType::Equal})) {
            auto equals = Previous();
            auto value = Assignment();

            if (expr->type == AstNodeType::Variable) {
                auto variable = static_cast<AstVariable*>(expr);
                auto name = variable->name;
                return new AstAssign{AstNodeType::Assign, name, value};
            } else if (expr->type == AstNodeType::Set) {
                auto get = static_cast<AstGet*>(expr);
                return new AstSet{AstNodeType::Set, get->object, get->name, value};
            }

            Error(equals, "Invalid assignment target.");
        }

        return expr;
    }

    AstNode* Parser::Or()
    {
        auto expr = And();

        while (Match({TokenType::Or})) {
            auto op = Previous();
            auto right = And();
            expr = new AstLogical{AstNodeType::Logical, op, expr, right};
        }
        return expr;
    }

    AstNode* Parser::And()
    {
        auto expr = Equality();

        while (Match({TokenType::And})) {
            auto op = Previous();
            auto right = Equality();
            expr = new AstLogical{AstNodeType::Logical, op, expr, right};
        }

        return expr;
    }

    AstNode* Parser::Equality()
    {
        auto expr = Comparison();

        while (Match({TokenType::BangEqual, TokenType::EqualEqual})) {
            auto op = Previous();
            auto right = Comparison();
            expr = new AstBinary{AstNodeType::Binary, op, expr, right};
        }

        return expr;
    }

    AstNode* Parser::Comparison()
    {
        auto expr = Term();

        while (Match({TokenType::Greater,
                TokenType::GreaterEqual, TokenType::Less, TokenType::LessEqual})) {
            auto op = Previous();
            auto right = Term();
            expr = new AstBinary{AstNodeType::Binary, op, expr, right};
        }

        return expr;
    }

    AstNode* Parser::Term()
    {
        auto expr = Factor();

        while (Match({TokenType::Minus, TokenType::Plus})) {
            auto op = Previous();
            auto right = Factor();
            expr = new AstBinary{AstNodeType::Binary, op, expr, right};
        }

        return expr;
    }

    AstNode* Parser::Factor()
    {
        auto expr = Unary();

        while (Match({TokenType::Slash, TokenType::Star})) {
            auto op = Previous();
            auto right = Unary();
            expr = new AstBinary{AstNodeType::Binary, op, expr, right};
        }

        return expr;
    }

    AstNode* Parser::Unary()
    {
        if (Match({TokenType::Bang, TokenType::Minus})) {
            auto op = Previous();
            auto right = Unary();
            return new AstUnary{AstNodeType::Unary, op, right};
        }

        return Call();
    }

    AstNode* Parser::Call()
    {
        auto expr = Primary();

        for (;;) {
            if (Match({TokenType::LeftParen})) {
                expr = FinishCall(expr);
            } else if (Match({TokenType::Dot})) {
                auto name = Consume(TokenType::Identifier, "Expect property name after '.'.");
                expr = new AstGet{AstNodeType::Get, expr, name};
            } else {
                break;
            }
        }

        return expr;
    }

    AstNode* Parser::FinishCall(AstNode* callee)
    {
        auto arguments = std::vector<AstNode*>();
        if (!Check(TokenType::RightParen)) {
            do {
                arguments.push_back(Expression());
            } while (Match({TokenType::Comma}));
        }

        auto paren = Consume(TokenType::RightParen, "Expect ')' after arguments.");

        return new AstCall{AstNodeType::Call, callee, paren, std::move(arguments)};
    }

    AstNode* Parser::Primary()
    {
        if (Match({
            TokenType::False, TokenType::True,
            TokenType::Nil, TokenType::Identifier,
            TokenType::Number, TokenType::String
        })) {
            return new AstLiteral{AstNodeType::Literal, Previous()};
        }

        if (Match({TokenType::LeftParen})) {
            auto expr = Expression();
            Consume(TokenType::RightParen, "Expect ')' after expression.");
            return expr;
        }

        throw Error(Peek(), "Expect expression.");
    }

}