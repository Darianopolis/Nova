#pragma once

#include "nova_Lang.hpp"

namespace nova
{
    Resolver::Resolver()
    {
        scopes.emplace_back();
        RegisterType(new Type{"nil"});
        RegisterType(new Type{"bool"});
        RegisterType(new Type{"float"});
        RegisterType(new Type{"int"});
        RegisterType(new Type{"string"});
        RegisterType(new Type{"function"});
    }

    void Resolver::RegisterType(Type* type)
    {
        scopes.front().insert({ type->name, type });
        type->registered = true;
    }

    void Resolver::RegisterGlobal(std::string_view name, Type* type)
    {
        scopes.front().insert({ name, type });
    }

    Type* Resolver::FindType(std::string_view name)
    {
        auto f = scopes.front().find(name);
        if (f == scopes.front().end()) {
            auto t = new Type{name};
            RegisterType(t);
            return t;
        }
        return f->second;
    }

    Type* Resolver::RecordAstType(AstNode* node, Type* type)
    {
        exprTypes.insert({ node, type });
        return type;
    }

    void Resolver::BeginScope()
    {
        NOVA_LOG("BeginScope");
        scopes.emplace_back();
    }

    void Resolver::EndScope()
    {
        NOVA_LOG("EndScope");
        scopes.pop_back();
    }

    void Resolver::Declare(Token* name)
    {
        if (scopes.empty()) return;

        auto& scope = scopes.back();
        if (scope.contains(name->lexeme)) {
            parser->scanner->compiler->Error(name, "Already a variable with this name in this scope.");
        }
        scope.insert({ name->lexeme, nullptr });
    }

    void Resolver::Define(Token* name, Type* type)
    {
        if (scopes.empty()) return;

        scopes.back()[name->lexeme] = type;
    }

    Type* Resolver::ResolveLocal(Token* name)
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

    void Resolver::Resolve()
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
}