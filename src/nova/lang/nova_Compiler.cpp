#include "nova_Lang.hpp"

namespace nova
{
    void Compiler::Error(i32 line, std::string_view message)
    {
        Report(line, "", message);
    }

    void Compiler::Error(const Token& token, std::string_view message)
    {
        if (token.type == TokenType::Eof) {
            Report(token.line, " at end", message);
        } else {
            Report(token.line, std::format(" at '{}'", token.lexeme), message);
        }
    }

    void Compiler::Report(i32 line, std::string_view where, std::string_view message)
    {
        NOVA_LOG("[line {}] Error{}: {}", line, where, message);
        hadError = true;
    }
}