#include "example_Main.hpp"

#include <nova/lang/nova_Lang.hpp>
#include <nova/lang/backends/nova_VulkanGlslBackend.hpp>

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

        NOVA_LOG("======== Tokens ========");

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

        NOVA_LOG("======== AST ========");

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

        NOVA_LOG("======== TYPES ========");

        nova::VulkanGlslBackend backend;
        backend.parser = &parser;

        backend.RegisterGlobal("gl_Position",    backend.FindType("vec4"));
        backend.RegisterGlobal("gl_VertexIndex", backend.FindType("uint"));

        auto uniforms = backend.FindType("Uniforms");
        uniforms->structure = new nova::Struct;
        uniforms->structure->members.insert({ "offset", backend.FindType("vec3") });

        auto vertices = backend.FindType("Vertex");
        vertices->structure = new nova::Struct;
        vertices->structure->members.insert({ "position", backend.FindType("vec3") });
        vertices->structure->members.insert({ "color",    backend.FindType("vec3") });

        auto rect = backend.FindType("ImRoundRect");
        rect->structure = new nova::Struct;
        rect->structure->members.insert({ "centerColor",   backend.FindType("vec4")  });
        rect->structure->members.insert({ "borderColor",   backend.FindType("vec4")  });
        rect->structure->members.insert({ "centerPos",     backend.FindType("vec2")  });
        rect->structure->members.insert({ "halfExtent",    backend.FindType("vec2")  });
        rect->structure->members.insert({ "cornerRadius",  backend.FindType("float") });
        rect->structure->members.insert({ "borderWidth",   backend.FindType("float") });
        rect->structure->members.insert({ "texTint",       backend.FindType("vec4")  });
        rect->structure->members.insert({ "texIndex",      backend.FindType("uint")  }); // image
        rect->structure->members.insert({ "texCenterPos",  backend.FindType("vec2")  });
        rect->structure->members.insert({ "texHalfExtent", backend.FindType("vec2")  });

        auto uniformAccessor = backend.RegisterAccessor(uniforms, nova::VulkanGlslBackend::AccessorMode::UniformBuffer,   true);
        auto vertexAccessor  = backend.RegisterAccessor(vertices, nova::VulkanGlslBackend::AccessorMode::BufferReference, true);
        auto rectAccessor    = backend.RegisterAccessor(rect,     nova::VulkanGlslBackend::AccessorMode::BufferReference, true);

        auto pc = backend.FindType("PushConstants");
        pc->structure = new nova::Struct;
        pc->structure->members.insert({ "uniforms", uniformAccessor->accessorType });
        pc->structure->members.insert({ "vertices", vertexAccessor->accessorType  });
        pc->structure->members.insert({ "rects",    rectAccessor->accessorType    });

        backend.RegisterType(pc);
        backend.RegisterGlobal("pc", pc);
        backend.RegisterGlobal("color", backend.FindType("vec3"));

        backend.Resolve();

        auto getTypeName = [&](nova::AstNode* node) {
            if (!backend.exprTypes.contains(node)) {
                return std::string_view("N/A");
            }
            return backend.exprTypes.at(node)->name;
        };

        auto printTypes = [&](this auto& self, nova::AstNode* _node, i32 depth) -> void {
            nova::VisitNode(nova::Overloads {
                [&](nova::AstFunction* node) {
                    INDENTED(0, ":fun<{}> {}\n", getTypeName(node), node->name->lexeme);
                    for (auto* cur = node->parameters.head; cur; cur = cur->next) {
                        self(cur, depth + 1);
                    }
                    self(node->body, depth + 1);
                },
                [&](nova::AstVarDecl* node) {
                    INDENTED(0, ":decl<{}>\n", getTypeName(node));
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
                    INDENTED(0, ":return<{}>\n", getTypeName(node));
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
                    INDENTED(0, ":varexpr<{}> {}\n", getTypeName(node), node->name->lexeme);
                },
                [&](nova::AstAssign* node) {
                    INDENTED(0, ":assign<{}>\n", getTypeName(node));
                    self(node->variable, depth + 1);
                    self(node->value, depth + 1);
                },
                [&](nova::AstGet* node) {
                    INDENTED(0, ":get<{}>\n", getTypeName(node));
                    self(node->object, depth + 1);
                    INDENTED(1, "{}\n", node->name->lexeme);
                },
                [&](nova::AstLogical* node) {
                    INDENTED(0, ":logical<{}> {}\n", getTypeName(node), node->op->lexeme);
                    self(node->left, depth + 1);
                    self(node->right, depth + 1);
                },
                [&](nova::AstBinary* node) {
                    INDENTED(0, ":binary<{}> {}\n", getTypeName(node), node->op->lexeme);
                    self(node->left, depth + 1);
                    self(node->right, depth + 1);
                },
                [&](nova::AstUnary* node) {
                    INDENTED(0, ":unary<{}> {}\n", getTypeName(node), node->op->lexeme);
                    self(node->right, depth + 1);
                },
                [&](nova::AstCall* node) {
                    INDENTED(0, ":call<{}>\n", getTypeName(node));
                    self(node->callee, depth + 1);
                    for (auto cur = node->arguments.head; cur; cur = cur->next) {
                        self(cur, depth + 1);
                    }
                },
                [&](nova::AstLiteral* node) {
                    INDENTED(0, ":literal<{}> {}\n", getTypeName(node), node->token->lexeme);
                },
                [&](nova::AstCondExpr* node) {
                    INDENTED(0, ":condexpr<{}>\n", getTypeName(node));
                    self(node->cond, depth + 1);
                    self(node->thenExpr, depth + 1);
                    self(node->elseExpr, depth + 1);
                }
            }, _node);
        };

        for (auto* cur = parser.nodes.head; cur; cur = cur->next) {
            printTypes(cur, 0);
        }

        NOVA_LOG("======== GLSL ========");

        backend.Generate(std::cout);
        std::cout << '\n';
    }
}