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

        compiler.hadError = false;

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

        auto indent = [](i32 depth) {
            for (i32 i = 0; i < depth; ++i) {
                std::cout << "  ";
            }
        };

#define INDENTED(offset, fmt, ...)do{\
            indent(depth + offset);std::cout << std::format(fmt __VA_OPT__(,) __VA_ARGS__);}while(0)

        auto writeAst = [&](this auto& self, nova::AstNode* _node, i32 depth) -> void {
            nova::VisitNode(nova::Overloads {
                [&](nova::AstFunction* node) {
                    INDENTED(0, ":fun {}\n", node->name->lexeme);
                    for (auto* cur = node->parameters.head; cur; cur = cur->next) {
                        self(cur, depth + 1);
                    }
                    self(node->body, depth + 1);
                },
                [&](nova::AstVarDecl* node) {
                    INDENTED(0, ":decl\n");
                    INDENTED(1, "{}", node->name->lexeme);
                    if (node->type) {
                        std::cout << std::format(": {}", node->type->lexeme);
                    }
                    std::cout << "\n";
                    self(node->initializer, depth + 1);
                },
                [&](nova::AstIf* node) {
                    INDENTED(0, ":if\n");
                    self(node->cond, depth + 1);
                    self(node->thenBranch, depth + 1);
                    self(node->elseBranch, depth + 1);
                },
                [&](nova::AstReturn* node) {
                    INDENTED(0, ":return\n");
                    self(node->value, depth + 1);
                },
                [&](nova::AstWhile* node) {
                    INDENTED(0, ":while\n");
                    self(node->condition, depth + 1);
                    self(node->body, depth + 1);
                },
                [&](nova::AstBlock* node) {
                    INDENTED(0, ":block\n");
                    for (auto cur = node->statements.head; cur; cur = cur->next) {
                        self(cur, depth + 1);
                    }
                },
                [&](nova::AstVariable* node) {
                    INDENTED(0, ":varexpr {}\n", node->name->lexeme);
                },
                [&](nova::AstAssign* node) {
                    INDENTED(0, ":assign\n");
                    self(node->variable, depth + 1);
                    self(node->value, depth + 1);
                },
                [&](nova::AstGet* node) {
                    INDENTED(0, ":get\n");
                    self(node->object, depth + 1);
                    INDENTED(1, "{}\n", node->name->lexeme);
                },
                [&](nova::AstLogical* node) {
                    INDENTED(0, ":logical {}\n", node->op->lexeme);
                    self(node->left, depth + 1);
                    self(node->right, depth + 1);
                },
                [&](nova::AstBinary* node) {
                    INDENTED(0, ":binary {}\n", node->op->lexeme);
                    self(node->left, depth + 1);
                    self(node->right, depth + 1);
                },
                [&](nova::AstUnary* node) {
                    INDENTED(0, ":unary {}\n", node->op->lexeme);
                    self(node->right, depth + 1);
                },
                [&](nova::AstCall* node) {
                    INDENTED(0, ":call\n");
                    self(node->callee, depth + 1);
                    for (auto cur = node->arguments.head; cur; cur = cur->next) {
                        self(cur, depth + 1);
                    }
                },
                [&](nova::AstLiteral* node) {
                    INDENTED(0, ":literal {}\n", node->token->lexeme);
                },
                [&](nova::AstCondExpr* node) {
                    INDENTED(0, ":condexpr\n");
                    self(node->cond, depth + 1);
                    self(node->thenExpr, depth + 1);
                    self(node->elseExpr, depth + 1);
                }
            }, _node);
        };

        for (auto* cur = parser.nodes.head; cur; cur = cur->next) {
            writeAst(cur, 0);
        }

        auto emitGlsl = [&](this auto& self, nova::AstNode* _node) -> void {
            nova::VisitNode(nova::Overloads {
                [&](nova::AstFunction* node) {
                    std::cout << (node->type ? node->type->lexeme : "void");
                    std::cout << " " << node->name->lexeme << "(";
                    for (auto* cur = node->parameters.head; cur; cur = cur->next) {
                        self(cur);
                        if (cur->next) {
                            std::cout << ",";
                        }
                    }
                    std::cout << ")";
                    self(node->body);
                },
                [&](nova::AstVarDecl* node) {
                    std::cout << node->type->lexeme;;
                    std::cout << " " << node->name->lexeme;
                    if (node->initializer) {
                        std::cout << "=";
                        self(node->initializer);
                    }
                },
                [&](nova::AstIf* node) {
                    std::cout << "if(";
                    self(node->cond);
                    std::cout << ")";
                    self(node->thenBranch);
                    if (node->elseBranch) {
                        std::cout << " else ";
                        self(node->elseBranch);
                    }
                },
                [&](nova::AstReturn* node) {
                    std::cout << "return ";
                    self(node->value);
                },
                [&](nova::AstWhile* node) {
                    std::cout << "while(";
                    self(node->condition);
                    std::cout << ")";
                    self(node->body);
                },
                [&](nova::AstBlock* node) {
                    std::cout << "{";
                    for (auto cur = node->statements.head; cur; cur = cur->next) {
                        self(cur);
                        std::cout << ";\n";
                    }
                    std::cout << "}";
                },
                [&](nova::AstVariable* node) {
                    std::cout << node->name->lexeme;
                },
                [&](nova::AstAssign* node) {
                    self(node->variable);
                    std::cout << "=";
                    self(node->value);
                },
                [&](nova::AstGet* node) {
                    self(node->object);
                    std::cout << "." << node->name->lexeme;
                },
                [&](nova::AstLogical* node) {
                    self(node->left);
                    std::cout << node->op->lexeme;
                    self(node->right);
                },
                [&](nova::AstBinary* node) {
                    self(node->left);
                    std::cout << node->op->lexeme;
                    self(node->right);
                },
                [&](nova::AstUnary* node) {
                    std::cout << node->op->lexeme;
                    self(node->right);
                },
                [&](nova::AstCall* node) {
                    self(node->callee);
                    std::cout << "(";
                    for (auto cur = node->arguments.head; cur; cur = cur->next) {
                        self(cur);
                        if (cur->next) {
                            std::cout << ",";
                        }
                    }
                    std::cout << ")";
                },
                [&](nova::AstLiteral* node) {
                    std::cout << node->token->lexeme;
                },
                [&](nova::AstCondExpr* node) {
                    self(node->cond);
                    std::cout << "\n?";
                    self(node->thenExpr);
                    std::cout << "\n:";
                    self(node->elseExpr);
                }
            }, _node);
        };

        for (auto* cur = parser.nodes.head; cur; cur = cur->next) {
            emitGlsl(cur);
        }
        std::cout << "\n";
    }
}