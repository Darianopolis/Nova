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
        return Peek()->type == type;
    }

    Token* Parser::Advance()
    {
        if (!IsAtEnd()) current++;
        return Previous();
    }

    bool Parser::IsAtEnd()
    {
        return Peek()->type == TokenType::Eof;
    }

    Token* Parser::Peek()
    {
        return &scanner->tokens[current];
    }

    Token* Parser::Previous()
    {
        return &scanner->tokens[current - 1];
    }

    Token* Parser::Consume(TokenType type, std::string_view message)
    {
        if (Check(type)) return Advance();

        throw Error(Peek(), message);
    }

    ParseError Parser::Error(Token* token, std::string_view message)
    {
        scanner->compiler->Error(token, message);
        return ParseError();
    }

    void Parser::Synchronize()
    {
        Advance();

        while (!IsAtEnd()) {
            if (Previous()->type == TokenType::Semicolon) return;

            switch (Peek()->type)
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
        AstNodeListBuilder list;
        try {
            while (!IsAtEnd()) {
                list.Append(Declaration());
            }
        } catch (ParseError) {
            NOVA_LOG("Parse completed with errors");
        }
        nodes = list.ToList();
    }

// -----------------------------------------------------------------------------

    AstNode* Parser::Declaration()
    {
        try {
            if (Match({TokenType::Fun})) return Function();
            if (Match({TokenType::Let})) {
                auto node = VarDeclaration();
                Consume(TokenType::Semicolon, "Expect ';' after variable declaration.");
                return node;
            }

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
        auto name = Consume(TokenType::Identifier, "Expect function name.");

        Consume(TokenType::LeftParen, "Expect '(' after function name.");
        AstNodeListBuilder parameters;
        if (!Check(TokenType::RightParen)) {
            do {
                parameters.Append(VarDeclaration());
            } while (Match({TokenType::Comma}));
        }

        Consume(TokenType::RightParen, "Expect ')' after parameters");

        Token* type = nullptr;
        if (Match({TokenType::Colon}) && !Match({TokenType::Void})) {
            type = Consume(TokenType::Identifier, "Expect identifier after explicit function return type declaration.");
        }

        Consume(TokenType::LeftBrace, "Expect '{' before function body.");
        auto body = Block();

        return new AstFunction{{AstNodeType::Function}, name, type, parameters.ToList(), body};
    }

    AstNode* Parser::VarDeclaration()
    {
        auto name = Consume(TokenType::Identifier, "Expect variable name.");

        Token* type = nullptr;
        if (Match({TokenType::Colon})) {
            type = Consume(TokenType::Identifier, "Expect identifier after explicit type declaration.");
        }

        auto initializer = Match({TokenType::Equal})
            ? Expression()
            : nullptr;

        return new AstVarDecl{{AstNodeType::VarDecl}, name, type, initializer};
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
        auto leftParen = Consume(TokenType::LeftParen, "Expect '(' after 'for'.");

        AstNode* initializer;
        if (Match({TokenType::Semicolon})) {
            initializer = nullptr;
        } else if (Match({TokenType::Let})) {
            initializer = VarDeclaration();
        } else {
            initializer = ExpressionStatement();
        }
        Consume(TokenType::Semicolon, "Expect ';' after loop initialization");

        auto condition = Check(TokenType::Semicolon) ? nullptr : Expression();
        Consume(TokenType::Semicolon, "Expect ';' after loop condition");

        auto increment = Check(TokenType::RightParen) ? nullptr : Expression();
        Consume(TokenType::RightParen, "Expect ')' after for clauses");

        auto body = Statement();

        if (increment) {
            AstNodeListBuilder builder;
            builder.Append(body);
            builder.Append(increment);
            body = new AstBlock{{AstNodeType::Block}, builder.ToList()};
        }

        if (!condition) {
            condition = new AstLiteral{{AstNodeType::Literal}, new Token{TokenType::True, leftParen->line, "true"}};
        }
        body = new AstWhile{{AstNodeType::While}, condition, body};

        if (initializer) {
            AstNodeListBuilder builder;
            builder.Append(initializer);
            builder.Append(body);
            body = new AstBlock{{AstNodeType::Block}, builder.ToList()};
        }

        return body;
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

        return new AstIf{{AstNodeType::If}, condition, thenBranch, elseBranch};
    }

    AstNode* Parser::ReturnStatement()
    {
        auto keyword = Previous();
        auto value = Check(TokenType::Semicolon) ? nullptr : Expression();
        Consume(TokenType::Semicolon, "Expect ';' after return statement.");
        return new AstReturn{{AstNodeType::Return}, keyword, value};
    }

    AstNode* Parser::WhileStatement()
    {
        Consume(TokenType::LeftParen, "Expect '(' after 'while'.");
        auto condition = Expression();
        Consume(TokenType::RightParen, "Expect ')' after condition.");
        auto body = Statement();

        return new AstWhile{{AstNodeType::While}, condition, body};
    }

    AstNode* Parser::Block()
    {
        AstNodeListBuilder statements;

        while (!Check(TokenType::RightBrace) && !IsAtEnd()) {
            statements.Append(Declaration());
        }

        Consume(TokenType::RightBrace, "Expect '}' after block.");
        return new AstBlock{{AstNodeType::Block}, statements.ToList()};
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
            auto value = Assignment();

            return new AstAssign{{AstNodeType::Assign}, expr, value};
        }

        if (Match({TokenType::PlusEqual, TokenType::MinusEqual,
                TokenType::StarEqual, TokenType::SlashEqual})) {
            auto op = Previous();
            auto right = Assignment();
            return new AstBinary{{AstNodeType::Binary}, op, expr, right};
        }

        if (Match({TokenType::QuestionMark})) {
            auto thenExpr = Assignment();
            Consume(TokenType::Colon, "Expect ':' in conditional expression");
            auto elseExpr = Assignment();

            return new AstCondExpr{{AstNodeType::CondExpr}, expr, thenExpr, elseExpr};
        }

        return expr;
    }

    AstNode* Parser::Or()
    {
        auto expr = And();

        while (Match({TokenType::PipePipe})) {
            auto op = Previous();
            auto right = And();
            expr = new AstLogical{{AstNodeType::Logical}, op, expr, right};
        }
        return expr;
    }

    AstNode* Parser::And()
    {
        auto expr = Equality();

        while (Match({TokenType::AmpersandAmpersand})) {
            auto op = Previous();
            auto right = Equality();
            expr = new AstLogical{{AstNodeType::Logical}, op, expr, right};
        }

        return expr;
    }

    AstNode* Parser::Equality()
    {
        auto expr = Comparison();

        while (Match({TokenType::BangEqual, TokenType::EqualEqual})) {
            auto op = Previous();
            auto right = Comparison();
            expr = new AstBinary{{AstNodeType::Binary}, op, expr, right};
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
            expr = new AstBinary{{AstNodeType::Binary}, op, expr, right};
        }

        return expr;
    }

    AstNode* Parser::Term()
    {
        auto expr = Factor();

        while (Match({TokenType::Minus, TokenType::Plus})) {
            auto op = Previous();
            auto right = Factor();
            expr = new AstBinary{{AstNodeType::Binary}, op, expr, right};
        }

        return expr;
    }

    AstNode* Parser::Factor()
    {
        auto expr = Unary();

        while (Match({TokenType::Slash, TokenType::Star})) {
            auto op = Previous();
            auto right = Unary();
            expr = new AstBinary{{AstNodeType::Binary}, op, expr, right};
        }

        return expr;
    }

    AstNode* Parser::Unary()
    {
        if (Match({TokenType::Bang, TokenType::Minus})) {
            auto op = Previous();
            auto right = Unary();
            return new AstUnary{{AstNodeType::Unary}, op, right};
        }

        return Call();
    }

    AstNode* Parser::Call()
    {
        auto expr = Primary();

        for (;;) {
            if (Match({TokenType::LeftParen})) {
                expr = FinishCall(expr, TokenType::RightParen);
            } else if (Match({TokenType::LeftBracket})) {
                expr = FinishCall(expr, TokenType::RightBracket);
            } else if (Match({TokenType::Dot})) {
                auto name = Consume(TokenType::Identifier, "Expect property name after '.'.");
                expr = new AstGet{{AstNodeType::Get}, expr, name};
            } else {
                break;
            }
        }

        return expr;
    }

    AstNode* Parser::FinishCall(AstNode* callee, TokenType closingToken)
    {
        AstNodeListBuilder arguments;
        if (!Check(closingToken)) {
            do {
                arguments.Append(Expression());
            } while (Match({TokenType::Comma}));
        }

        auto paren = Consume(closingToken, closingToken == TokenType::RightParen
            ? "Expect ')' after arguments." : "Expect ']' after arguments.");

        return new AstCall{{AstNodeType::Call}, callee, paren, arguments.ToList()};
    }

    AstNode* Parser::Primary()
    {
        if (Match({ TokenType::False, TokenType::True,
                TokenType::Nil,  TokenType::Number, TokenType::String })) {
            return new AstLiteral{{AstNodeType::Literal}, Previous()};
        }

        if (Match({TokenType::Identifier})) {
            return new AstVariable{{AstNodeType::Variable}, Previous()};
        }

        if (Match({TokenType::LeftParen})) {
            auto expr = Expression();
            Consume(TokenType::RightParen, "Expect ')' after expression.");
            return expr;
        }

        throw Error(Peek(), "Expect expression.");
    }

}