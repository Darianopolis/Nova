#include "nova_VulkanGlslBackend.hpp"

namespace nova
{
    VulkanGlslBackend::VulkanGlslBackend()
    {
        scopes.emplace_back();
        RegisterType(new Type{"nil"});
        RegisterType(new Type{"bool"});
        RegisterType(new Type{"float"});
        RegisterType(new Type{"int"});
        RegisterType(new Type{"string"});
        RegisterType(new Type{"function"});
    }

    void VulkanGlslBackend::RegisterType(Type* type)
    {
        scopes.front().insert({ type->name, type });
        type->registered = true;
    }

    void VulkanGlslBackend::RegisterGlobal(std::string_view name, Type* type)
    {
        scopes.front().insert({ name, type });
    }

    Type* VulkanGlslBackend::FindType(std::string_view name)
    {
        auto f = scopes.front().find(name);
        if (f == scopes.front().end()) {
            auto t = new Type{name};
            RegisterType(t);
            return t;
        }
        return f->second;
    }

    Type* VulkanGlslBackend::RecordAstType(AstNode* node, Type* type)
    {
        exprTypes.insert({ node, type });
        return type;
    }

    void VulkanGlslBackend::BeginScope()
    {
        NOVA_LOG("BeginScope");
        scopes.emplace_back();
    }

    void VulkanGlslBackend::EndScope()
    {
        NOVA_LOG("EndScope");
        scopes.pop_back();
    }

    void VulkanGlslBackend::Declare(Token* name)
    {
        if (scopes.empty()) return;

        auto& scope = scopes.back();
        if (scope.contains(name->lexeme)) {
            parser->scanner->compiler->Error(name, "Already a variable with this name in this scope.");
        }
        scope.insert({ name->lexeme, nullptr });
    }

    void VulkanGlslBackend::Define(Token* name, Type* type)
    {
        if (scopes.empty()) return;

        scopes.back()[name->lexeme] = type;
    }

    Type* VulkanGlslBackend::ResolveLocal(Token* name)
    {
        for (i64 i = i64(scopes.size()) - 1; i >= 0; --i) {
            auto f = scopes[i].find(name->lexeme);
            if (f != scopes[i].end()) {
                return f->second;
            }
        }

        parser->scanner->compiler->Error(name, "Can't find type for variable.");
        return FindType("nil");
    }

    void VulkanGlslBackend::Resolve()
    {
        auto resolveTypes = [&](this auto& self, nova::AstNode* _node) -> Type* {
            return nova::VisitNode<Type*>(nova::Overloads {
                [&](nova::AstFunction* node) {
                    auto enclosing = currentFnType;
                    Type* type = FindType("nil");
                    currentFnType = &type;
                    NOVA_CLEANUP(&) { currentFnType = enclosing; };

                    Declare(node->name);
                    Define(node->name, *currentFnType);

                    BeginScope();

                    for (auto cur = node->parameters.head; cur; cur = cur->next) {
                        self(cur);
                    }

                    self(node->body);

                    EndScope();

                    Define(node->name, *currentFnType);
                    NOVA_LOGEXPR(node->name);
                    NOVA_LOG("Defined fn - {} -> ", node->name->lexeme, scopes.back()[node->name->lexeme]->name);
                    return RecordAstType(node, *currentFnType);
                },
                [&](nova::AstVarDecl* node) {
                    Type* type = nullptr;
                    if (node->initializer) {
                        type = self(node->initializer);
                    }
                    if (node->type) {
                        type = FindType(node->type->lexeme);
                    }
                    Declare(node->name);
                    Define(node->name, type);
                    return RecordAstType(node, type);
                },
                [&](nova::AstIf* node) {
                    self(node->cond);
                    self(node->thenBranch);
                    self(node->elseBranch);
                    return FindType("nil");
                },
                [&](nova::AstReturn* node) {
                    if (!currentFnType) {
                        parser->scanner->compiler->Error(node->keyword, "Can't return outside of function.");
                    }

                    auto type = FindType("nil");
                    if (node->value) {
                        type = self(node->value);
                        *currentFnType = type;
                    }

                    return RecordAstType(node, type);
                },
                [&](nova::AstWhile* node) {
                    self(node->condition);
                    self(node->body);

                    return FindType("nil");
                },
                [&](nova::AstBlock* node) {
                    for (auto cur = node->statements.head; cur; cur = cur->next) {
                        self(cur);
                    }

                    return RecordAstType(node, FindType("nil"));
                },
                [&](nova::AstVariable* node) {
                    // TODO: Handle global and lexical scope
                    return RecordAstType(node, ResolveLocal(node->name));
                },
                [&](nova::AstAssign* node) {
                    self(node->value);
                    return RecordAstType(node, self(node->variable));
                },
                [&](nova::AstGet* node) {
                    auto type = self(node->object);
                    NOVA_LOGEXPR(type);
                    NOVA_LOGEXPR(type->structure);
                    NOVA_LOGEXPR(node->name->lexeme);
                    for (auto&[key, t] : type->structure->members) {
                        NOVA_LOGEXPR(t);
                        NOVA_LOGEXPR(t->name);
                    }
                    if (type && type->structure
                            && type->structure->members.contains(node->name->lexeme)) {
                        return RecordAstType(node, type->structure->members.at(node->name->lexeme));
                    }
                    return RecordAstType(node, FindType("nil"));
                },
                [&](nova::AstLogical* node) {
                    // Assume both sides are equivalently typed
                    auto type = self(node->left);
                    self(node->right);
                    return RecordAstType(node, type);
                },
                [&](nova::AstBinary* node) {
                    // Assume both sides are equivalently typed
                    auto type = self(node->left);
                    self(node->right);
                    return RecordAstType(node, type);
                },
                [&](nova::AstUnary* node) {
                    return self(node->right);
                },
                [&](nova::AstCall* node) {
                    auto type = self(node->callee);
                    for (auto cur = node->arguments.head; cur; cur = cur->next) {
                        self(cur);
                    }
                    return RecordAstType(node, type);
                },
                [&](nova::AstLiteral* node) {
                    auto type = FindType("nil");

                    switch (node->token->type)
                    {
                    break;case TokenType::True:
                          case TokenType::False:
                        type = FindType("bool");

                    break;case TokenType::Number:
                        if (node->token->lexeme.contains('.')) {
                            type = FindType("float");
                        } else {
                            type = FindType("int");
                        }

                    break;case TokenType::String:
                        type = FindType("string");
                    }

                    RecordAstType(node, type);
                    return type;
                },
                [&](nova::AstCondExpr* node) {
                    self(node->cond);

                    // Always use then branch for type for now
                    auto type = self(node->thenExpr);

                    self(node->elseExpr);

                    return RecordAstType(node, type);
                }
            }, _node);
        };

        for (auto* cur = parser->nodes.head; cur; cur = cur->next) {
            resolveTypes(cur);
        }
    }

    void VulkanGlslBackend::Generate(std::ostream& out)
    {
        auto emitGlsl = [&](this auto& self, nova::AstNode* _node) -> void {
            nova::VisitNode(nova::Overloads {
                [&](nova::AstFunction* node) {
                    out << (node->type ? node->type->lexeme : "void");
                    out << " " << node->name->lexeme << "(";
                    for (auto* cur = node->parameters.head; cur; cur = cur->next) {
                        self(cur);
                        if (cur->next) {
                            out << ",";
                        }
                    }
                    out << ")";
                    self(node->body);
                },
                [&](nova::AstVarDecl* node) {
                    // if (node->type) {
                    //     out << node->type->lexeme;
                    // } else {
                    //     out << "auto";
                    // }
                    auto type = exprTypes.at(node)->name;
                    if (type == "Uniforms") {
                        out << "uvec2";
                    } else if (type == "Vertex") {
                        out << type << "_readonly_buffer_reference";
                    } else {
                        out << type;
                    }
                    out << " " << node->name->lexeme;
                    if (node->initializer) {
                        out << "=";
                        self(node->initializer);
                    }
                },
                [&](nova::AstIf* node) {
                    out << "if(";
                    self(node->cond);
                    out << ")";
                    self(node->thenBranch);
                    if (node->elseBranch) {
                        out << " else ";
                        self(node->elseBranch);
                    }
                },
                [&](nova::AstReturn* node) {
                    out << "return ";
                    self(node->value);
                },
                [&](nova::AstWhile* node) {
                    out << "while(";
                    self(node->condition);
                    out << ")";
                    self(node->body);
                },
                [&](nova::AstBlock* node) {
                    out << "{";
                    for (auto cur = node->statements.head; cur; cur = cur->next) {
                        self(cur);
                        out << ";\n";
                    }
                    out << "}";
                },
                [&](nova::AstVariable* node) {
                    out << node->name->lexeme;
                },
                [&](nova::AstAssign* node) {
                    self(node->variable);
                    out << "=";
                    self(node->value);
                },
                [&](nova::AstGet* node) {
                    // self(node->object);
                    auto type = exprTypes.at(node->object)->name;
                    if (type == "Uniforms") {
                        out << "Uniforms_readonly_uniform_buffer[";
                        self(node->object);
                        out << ".x].data[";
                        self(node->object);
                        out << ".y]";
                    } else if (type == "Vertex") {
                        self(node->object);
                        out << ".get";
                    } else {
                        self(node->object);
                    }
                    out << "." << node->name->lexeme;
                },
                [&](nova::AstLogical* node) {
                    self(node->left);
                    out << node->op->lexeme;
                    self(node->right);
                },
                [&](nova::AstBinary* node) {
                    self(node->left);
                    out << node->op->lexeme;
                    self(node->right);
                },
                [&](nova::AstUnary* node) {
                    out << node->op->lexeme;
                    self(node->right);
                },
                [&](nova::AstCall* node) {
                    self(node->callee);
                    out << "(";
                    for (auto cur = node->arguments.head; cur; cur = cur->next) {
                        self(cur);
                        if (cur->next) {
                            out << ",";
                        }
                    }
                    out << ")";
                },
                [&](nova::AstLiteral* node) {
                    out << node->token->lexeme;
                },
                [&](nova::AstCondExpr* node) {
                    self(node->cond);
                    out << "\n?";
                    self(node->thenExpr);
                    out << "\n:";
                    self(node->elseExpr);
                }
            }, _node);
        };

        for (auto* cur = parser->nodes.head; cur; cur = cur->next) {
            emitGlsl(cur);
            std::cout << ";\n";
        }

        out.flush();
    }
}