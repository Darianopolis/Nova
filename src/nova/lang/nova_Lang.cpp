#include "nova_Lang.hpp"

namespace nova
{
    std::string_view ToString(TokenType type)
    {
        switch (type)
        {
        case TokenType::None: return "None";

        case TokenType::LeftParen: return "LeftParen";
        case TokenType::RightParen: return "RightParen";
        case TokenType::LeftBrace: return "LeftBrace";
        case TokenType::RightBrace: return "RightBrace";
        case TokenType::LeftBracket: return "LeftBracket";
        case TokenType::RightBracket: return "RightBracket";
        case TokenType::Comma: return "Comma";
        case TokenType::Dot: return "Dot";
        case TokenType::Colon: return "Colon";
        case TokenType::QuestionMark: return "QuestionMark";
        case TokenType::Plus: return "Plus";
        case TokenType::PlusEqual: return "PlusEqual";
        case TokenType::Minus: return "Minus";
        case TokenType::MinusEqual: return "MinusEqual";
        case TokenType::Star: return "Star";
        case TokenType::StarEqual: return "StarEqual";
        case TokenType::Slash: return "Slash";
        case TokenType::SlashEqual: return "SlashEqual";
        case TokenType::Caret: return "Caret";
        case TokenType::Semicolon: return "Semicolon";
        case TokenType::Ampersand: return "Ampersand";
        case TokenType::AmpersandAmpersand: return "AmpersandAmpersand";
        case TokenType::Pipe: return "Pipe";
        case TokenType::PipePipe: return "PipePipe";
        case TokenType::Bang: return "Bang";
        case TokenType::BangEqual: return "BangEqual";
        case TokenType::Equal: return "Equal";
        case TokenType::EqualEqual: return "EqualEqual";
        case TokenType::Greater: return "Greater";
        case TokenType::GreaterEqual: return "GreaterEqual";
        case TokenType::Less: return "Less";
        case TokenType::LessEqual: return "LessEqual";
        case TokenType::Identifier: return "Identifier";
        case TokenType::String: return "String";
        case TokenType::Number: return "Number";
        case TokenType::Else: return "Else";
        case TokenType::False: return "False";
        case TokenType::For: return "For";
        case TokenType::If: return "If";
        case TokenType::Nil: return "Nil";
        case TokenType::Return: return "Return";
        case TokenType::True: return "True";
        case TokenType::While: return "While";
        case TokenType::Void: return "Void";
        case TokenType::Fun: return "Fun";
        case TokenType::Let: return "Let";

        case TokenType::Eof: return "Eof";
        }

        std::unreachable();
    }

// -----------------------------------------------------------------------------

    AstNodeListBuilder::AstNodeListBuilder(AstNodeList list)
        : head(list.head)
        , tail(list.head)
    {
        if (!tail) return;

        while (tail->next) {
            tail = tail->next;
        }
    }

    void AstNodeListBuilder::Append(AstNode* toAppend)
    {
        if (!toAppend) return;

        if (!head) {
            head = tail = toAppend;
        } else {
            tail->next = toAppend;
            tail = tail->next;
        }
    }

    AstNodeList AstNodeListBuilder::ToList() const
    {
        return{ head };
    }
}