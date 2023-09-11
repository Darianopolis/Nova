#include "example_Main.hpp"

#include <nova/lang/nova_Lang.hpp>

NOVA_EXAMPLE(Lang, "lang")
{
    nova::Compiler compiler;

    std::string source;
    std::string line;
    for (;;) {
        source.clear();
        for (;;) {
            std::cout << "> ";
            if (!std::getline(std::cin, line)) {
                break;
            }

            if (line.empty() || !line.ends_with('\\')) {
                source.append(line);
                break;
            } else {
                source.append(line.begin(), line.end() - 1);
                source.push_back('\n');
            }
        }

        nova::Scanner scanner;
        scanner.compiler = &compiler;
        scanner.source = source;

        scanner.ScanTokens();

        i32 lastLine = -1;
        for (auto& token : scanner.tokens) {
            NOVA_LOG("{:0>4} {: >18} {}",
                token.line == lastLine
                    ? "   |"
                    : std::to_string(token.line),
                nova::ToString(token.type),
                token.lexeme);

            lastLine = token.line;
        }

        nova::Parser parser;
        parser.scanner = &scanner;

        parser.Parse();
        NOVA_LOG("Parsed");
    }
}