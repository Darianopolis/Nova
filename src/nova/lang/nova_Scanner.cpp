#include "nova_Lang.hpp"

namespace nova
{
    TokenType GetKeywordType(std::string_view keyword) {
        static const auto Keywords = [] {
            std::unordered_map<std::string_view, TokenType> keywords;

            keywords.insert({ "for",    TokenType::For    });
            keywords.insert({ "if",     TokenType::If     });
            keywords.insert({ "return", TokenType::Return });
            keywords.insert({ "else",   TokenType::Else   });
            keywords.insert({ "true",   TokenType::True   });
            keywords.insert({ "false",  TokenType::False  });
            keywords.insert({ "while",  TokenType::While  });
            keywords.insert({ "void",   TokenType::Void   });
            keywords.insert({ "let",    TokenType::Let    });
            keywords.insert({ "fn",     TokenType::Fun    });

            return keywords;
        }();

        auto type = Keywords.find(keyword);

        return type == Keywords.end()
            ? TokenType::Identifier
            : type->second;
    }

    bool Scanner::IsAlpha(char c)
    {
        return (c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z')
            ||  c == '_';
    }

    bool Scanner::IsDigit(char c)
    {
        return c >= '0' && c <= '9';
    }

    bool Scanner::IsAlphaNumeric(char c)
    {
        return IsAlpha(c) || IsDigit(c);
    }

// -----------------------------------------------------------------------------

    bool Scanner::IsAtEnd()
    {
        return current >= source.size();
    }

    void Scanner::ScanTokens()
    {
        while (!IsAtEnd()) {
            // We are at the beginning of the next lexeme
            start = current;
            ScanToken();
        }

        tokens.emplace_back(TokenType::Eof, line, "");
    }

    void Scanner::ScanToken()
    {
        auto c = Advance();
        switch (c)
        {
        break;case '(': AddToken(TokenType::LeftParen);
        break;case ')': AddToken(TokenType::RightParen);
        break;case '{': AddToken(TokenType::LeftBrace);
        break;case '}': AddToken(TokenType::RightBrace);
        break;case '[': AddToken(TokenType::LeftBracket);
        break;case ']': AddToken(TokenType::RightBracket);

        break;case ',': AddToken(TokenType::Comma);
        break;case '.': AddToken(TokenType::Dot);
        break;case ':': AddToken(TokenType::Colon);
        break;case '?': AddToken(TokenType::QuestionMark);
        break;case '+': AddToken(Match('=') ? TokenType::PlusEqual : TokenType::Plus);
        break;case '-': AddToken(Match('=') ? TokenType::MinusEqual : TokenType::Minus);
        break;case '*': AddToken(Match('=') ? TokenType::StarEqual : TokenType::Star);
        break;case '^': AddToken(TokenType::Caret);
        break;case ';': AddToken(TokenType::Semicolon);

        break;case '&': AddToken(Match('&') ? TokenType::AmpersandAmpersand : TokenType::Ampersand);
        break;case '|': AddToken(Match('|') ? TokenType::PipePipe : TokenType::Pipe);
        break;case '!': AddToken(Match('=') ? TokenType::BangEqual : TokenType::Bang);
        break;case '=': AddToken(Match('=') ? TokenType::EqualEqual : TokenType::Equal);
        break;case '>': AddToken(Match('=') ? TokenType::GreaterEqual : TokenType::Greater);
        break;case '<': AddToken(Match('=') ? TokenType::LessEqual : TokenType::Less);

        break;case '/':
            if (Match('=')) {
                AddToken(TokenType::SlashEqual);
            } else if (Match('/')) {
                // only line comments
                while (Peek() != '\n' && !IsAtEnd()) Advance();
            } else {
                AddToken(TokenType::Slash);
            }

        break;case ' ': case '\r': case '\t': // Whitespace
        break;case '\n': line++;

        break;case '"': String();

        break;default:
            if (IsDigit(c)) {
                Number();
            } else if (IsAlpha(c)) {
                Identifier();
            } else {
                compiler->Error(line, "Unexpected character.");
            }
        }
    }

    void Scanner::AddToken(TokenType type)
    {
        tokens.emplace_back(type, line, source.substr(start, current - start));
    }

    char Scanner::Advance()
    {
        return source[current++];
    }

    bool Scanner::Match(char expected)
    {
        if (IsAtEnd()) return false;
        if (source[current] != expected) return false;

        current++;
        return true;
    }

    char Scanner::Peek()
    {
        if (IsAtEnd()) return '\0';
        return source[current];
    }

    char Scanner::PeekNext()
    {
        if (current + 1 >= source.size()) return '\0';
        return source[current + 1];
    }

    void Scanner::String()
    {
        while (Peek() != '"' && !IsAtEnd()) {
            if (Peek() == '\n') line++;
            Advance();
        }

        if (IsAtEnd()) {
            compiler->Error(line, "Unterminated string.");
            return;
        }

        Advance();

        tokens.emplace_back(TokenType::String, line,
            source.substr(start + 1, current - start - 1));
    }

    void Scanner::Number()
    {
        while (IsDigit(Peek())) Advance();

        if (Peek() == '.' && IsDigit(PeekNext())) {
            Advance();

            while (IsDigit(Peek())) Advance();
        }

        AddToken(TokenType::Number);
    }

    void Scanner::Identifier()
    {
        while (IsAlphaNumeric(Peek())) Advance();

        auto text = source.substr(start, current - start);
        AddToken(GetKeywordType(text));
    }
}