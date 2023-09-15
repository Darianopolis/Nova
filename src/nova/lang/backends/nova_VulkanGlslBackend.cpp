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

    void VulkanGlslBackend::RegisterAccessor(Accessor accessor)
    {
        auto key = std::string_view(accessor.name);
        accessors.insert({ key, std::move(accessor) });
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
        auto resolveTypes = [&](this auto& self, AstNode* _node) -> Type* {
            return VisitNode<Type*>(Overloads {
                [&](AstFunction* node) {
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
                [&](AstVarDecl* node) {
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
                [&](AstIf* node) {
                    self(node->cond);
                    self(node->thenBranch);
                    self(node->elseBranch);
                    return FindType("nil");
                },
                [&](AstReturn* node) {
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
                [&](AstWhile* node) {
                    self(node->condition);
                    self(node->body);

                    return FindType("nil");
                },
                [&](AstBlock* node) {
                    for (auto cur = node->statements.head; cur; cur = cur->next) {
                        self(cur);
                    }

                    return RecordAstType(node, FindType("nil"));
                },
                [&](AstVariable* node) {
                    // TODO: Handle global and lexical scope
                    return RecordAstType(node, ResolveLocal(node->name));
                },
                [&](AstAssign* node) {
                    NOVA_LOGEXPR(i32(node->nodeType));
                    self(node->value);
                    auto type = RecordAstType(node, self(node->variable));
                    return type;
                },
                [&](AstGet* node) {
                    auto type = self(node->object);
                    if (accessors.contains(type->name)) {
                        type = accessors.at(type->name).element;
                    }
                    if (type && type->structure
                            && type->structure->members.contains(node->name->lexeme)) {
                        return RecordAstType(node, type->structure->members.at(node->name->lexeme));
                    }
                    return RecordAstType(node, FindType("nil"));
                },
                [&](AstLogical* node) {
                    // Assume both sides are equivalently typed
                    auto type = self(node->left);
                    self(node->right);
                    return RecordAstType(node, type);
                },
                [&](AstBinary* node) {
                    // Assume both sides are equivalently typed
                    auto type = self(node->left);
                    self(node->right);
                    return RecordAstType(node, type);
                },
                [&](AstUnary* node) {
                    return self(node->right);
                },
                [&](AstCall* node) {
                    auto type = self(node->callee);
                    for (auto cur = node->arguments.head; cur; cur = cur->next) {
                        self(cur);
                    }
                    return RecordAstType(node, type);
                },
                [&](AstLiteral* node) {
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
                [&](AstCondExpr* node) {
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

    template<class Recurse>
    void EmitAccess(VulkanGlslBackend& backend, std::ostream& out, AstNode* node, Recurse& recurse)
    {
        auto type = backend.exprTypes.at(node)->name;
        if (backend.accessors.contains(type)) {
            auto& accessor = backend.accessors.at(type);
            switch (accessor.type)
            {
            break;case VulkanGlslBackend::AccessorType::BufferReference:
                recurse(node);
                out << ".get";
            break;case VulkanGlslBackend::AccessorType::StorageBuffer:
                  case VulkanGlslBackend::AccessorType::UniformBuffer:
                out << type << "[";
                recurse(node);
                out << ".x].data[";
                recurse(node);
                out << ".y]";
            }
        } else {
            recurse(node);
        }
    }

    void VulkanGlslBackend::Generate(std::ostream& out)
    {
        auto emitGlsl = [&](this auto& self, AstNode* _node) -> void {
            VisitNode(Overloads {
                [&](AstFunction* node) {
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
                [&](AstVarDecl* node) {
                    auto type = exprTypes.at(node)->name;
                    if (accessors.contains(type)) {
                        auto& accessor = accessors.at(type);
                        switch (accessor.type)
                        {
                        break;case AccessorType::BufferReference:
                            out << accessor.name;
                        break;case AccessorType::StorageBuffer:
                              case AccessorType::UniformBuffer:
                            out << "uvec2";
                        }
                    } else {
                        out << type;
                    }
                    out << " " << node->name->lexeme;
                    if (node->initializer) {
                        out << "=";
                        if (!accessors.contains(type)) {
                            EmitAccess(*this, out, node->initializer, self);
                        } else {
                            self(node->initializer);
                        }
                    }
                },
                [&](AstIf* node) {
                    out << "if(";
                    self(node->cond);
                    out << ")";
                    self(node->thenBranch);
                    if (node->elseBranch) {
                        out << " else ";
                        self(node->elseBranch);
                    }
                },
                [&](AstReturn* node) {
                    out << "return ";
                    self(node->value);
                },
                [&](AstWhile* node) {
                    out << "while(";
                    self(node->condition);
                    out << ")";
                    self(node->body);
                },
                [&](AstBlock* node) {
                    out << "{";
                    for (auto cur = node->statements.head; cur; cur = cur->next) {
                        self(cur);
                        out << ";\n";
                    }
                    out << "}";
                },
                [&](AstVariable* node) {
                    out << node->name->lexeme;
                },
                [&](AstAssign* node) {
                    auto lhsHasAccessor = accessors.contains(exprTypes.at(node->variable)->name);
                    auto rhsHasAccessor = accessors.contains(exprTypes.at(node->value)->name);
                    if (lhsHasAccessor == rhsHasAccessor) {
                        self(node->variable);
                        out << "=";
                        self(node->value);
                    } else {
                        EmitAccess(*this, out, node->variable, self);
                        out << "=";
                        EmitAccess(*this, out, node->value, self);
                    }
                },
                [&](AstGet* node) {
                    auto type = exprTypes.at(node->object)->name;
                    EmitAccess(*this, out, node->object, self);
                    out << "." << node->name->lexeme;
                },
                [&](AstLogical* node) {
                    self(node->left);
                    out << node->op->lexeme;
                    self(node->right);
                },
                [&](AstBinary* node) {
                    self(node->left);
                    out << node->op->lexeme;
                    self(node->right);
                },
                [&](AstUnary* node) {
                    out << node->op->lexeme;
                    self(node->right);
                },
                [&](AstCall* node) {
                    bool isVecAccessor = accessors.contains(exprTypes.at(node->callee)->name);
                    if (isVecAccessor) {
                        out << "(";
                    }
                    self(node->callee);
                    if (isVecAccessor) {
                        out << "+vec2(0,";
                    } else {
                        out << (node->paren->type == TokenType::RightParen ? "(" : "[");
                    }
                    for (auto cur = node->arguments.head; cur; cur = cur->next) {
                        self(cur);
                        if (cur->next) {
                            out << ",";
                        }
                    }
                    if (isVecAccessor) {
                        out << "))";
                    } else {
                        out << (node->paren->type == TokenType::RightParen ? ")" : "]");
                    }
                },
                [&](AstLiteral* node) {
                    out << node->token->lexeme;
                },
                [&](AstCondExpr* node) {
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