#include "nova_VulkanGlslBackend.hpp"

namespace nova
{
    VulkanGlslBackend::VulkanGlslBackend()
    {
        scopes.emplace_back();
    }

    VulkanGlslBackend::ImageAccessor* VulkanGlslBackend::RegisterImageAccessor(std::string_view format, i32 dims, AccessorMode mode)
    {
        const char* prefix;
        switch (mode)
        {
        break;case AccessorMode::SampledImage:
            prefix = "SampledImage";
        break;case AccessorMode::StorageImage:
            prefix = "StorageImage";
        }

        auto name = std::format("{}{}D{}{}", prefix, dims, format.empty() ? "" : "_", format);

        if (bufferAccessors.contains(name)) {
            return imageAccessors.at(name);
        }

        auto accessor = new ImageAccessor {
            .format = format,
            .dims = dims,
            .mode = mode,
            .name = std::move(name),
        };

        accessor->accessorType = FindType(accessor->name);

        auto key = std::string_view(accessor->name);
        imageAccessors.insert({ key, accessor });

        return accessor;
    }

    VulkanGlslBackend::BufferAccessor* VulkanGlslBackend::RegisterBufferAccessor(Type* element, AccessorMode mode, bool readonly)
    {
        const char* suffix;
        switch (mode)
        {
        break;case AccessorMode::BufferReference:
            suffix = "buffer_reference";
        break;case AccessorMode::UniformBuffer:
            suffix = "uniform_buffer";
        break;case AccessorMode::StorageBuffer:
            suffix = "storage_buffer";
        break;default:
            std::unreachable();
        }

        auto name = std::format("{}{}_{}",
                element->name,
                readonly ? "_readonly" : "",
                suffix);

        if (bufferAccessors.contains(name)) {
            return bufferAccessors.at(name);
        }

        auto accessor = new BufferAccessor {
            .element = element,
            .mode = mode,
            .readonly = readonly,
            .name = std::move(name),
        };

        accessor->accessorType = FindType(accessor->name);

        auto key = std::string_view(accessor->name);
        bufferAccessors.insert({ key, accessor });

        return accessor;
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
                    if (bufferAccessors.contains(type->name)) {
                        auto elemType = bufferAccessors.at(type->name)->element;
                        if (elemType) {
                            type = elemType;
                        }
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

    void VulkanGlslBackend::PrintAst()
    {
        auto indent = [](i32 depth) {
            for (i32 i = 0; i < depth; ++i) {
                std::cout << "  ";
            }
        };

#define INDENTED(offset, fmt, ...)do{\
            indent(depth + offset);std::cout << std::format(fmt __VA_OPT__(,) __VA_ARGS__);}while(0)

        auto getTypeName = [&](nova::AstNode* node) {
            if (!exprTypes.contains(node)) {
                return std::string_view("N/A");
            }
            return exprTypes.at(node)->name;
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

        for (auto* cur = parser->nodes.head; cur; cur = cur->next) {
            printTypes(cur, 0);
        }
    }

    template<class Recurse>
    void EmitAccess(VulkanGlslBackend& backend, std::ostream& out, AstNode* node, Recurse& recurse)
    {
        auto type = backend.exprTypes.at(node)->name;
        if (backend.bufferAccessors.contains(type)) {
            auto& accessor = backend.bufferAccessors.at(type);
            switch (accessor->mode)
            {
            break;case VulkanGlslBackend::AccessorMode::BufferReference:
                recurse(node);
                out << ".get";
            break;case VulkanGlslBackend::AccessorMode::StorageBuffer:
                  case VulkanGlslBackend::AccessorMode::UniformBuffer:
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
                            out << ", ";
                        }
                    }
                    out << ")";
                    self(node->body);
                },
                [&](AstVarDecl* node) {
                    auto type = exprTypes.at(node)->name;
                    if (bufferAccessors.contains(type)) {
                        auto* accessor = bufferAccessors.at(type);
                        switch (accessor->mode)
                        {
                        break;case AccessorMode::BufferReference:
                            out << accessor->name;
                        break;case AccessorMode::StorageBuffer:
                              case AccessorMode::UniformBuffer:
                            out << "uvec2";
                        }
                    } else if (imageAccessors.contains(type)) {
                        out << "uvec2";
                    } else {
                        out << type;
                    }
                    out << " " << node->name->lexeme;
                    if (node->initializer) {
                        out << " = ";
                        if (!bufferAccessors.contains(type)) {
                            EmitAccess(*this, out, node->initializer, self);
                        } else {
                            self(node->initializer);
                        }
                    }
                },
                [&](AstIf* node) {
                    out << "if (";
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
                    out << "while (";
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
                    auto lhsHasAccessor = bufferAccessors.contains(exprTypes.at(node->variable)->name);
                    auto rhsHasAccessor = bufferAccessors.contains(exprTypes.at(node->value)->name);
                    if (lhsHasAccessor == rhsHasAccessor) {
                        self(node->variable);
                        out << " = ";
                        self(node->value);
                    } else {
                        EmitAccess(*this, out, node->variable, self);
                        out << " = ";
                        EmitAccess(*this, out, node->value, self);
                    }
                },
                [&](AstGet* node) {
                    auto type = exprTypes.at(node->object)->name;
                    EmitAccess(*this, out, node->object, self);
                    out << "." << node->name->lexeme;
                },
                [&](AstLogical* node) {
                    out << "((";
                    self(node->left);
                    out << ") " << node->op->lexeme << " (";
                    self(node->right);
                    out << "))";
                },
                [&](AstBinary* node) {
                    out << "((";
                    self(node->left);
                    out << ") " << node->op->lexeme << " (";
                    self(node->right);
                    out << "))";
                },
                [&](AstUnary* node) {
                    out << "(";
                    out << node->op->lexeme;
                    self(node->right);
                    out << ")";
                },
                [&](AstCall* node) {
                    auto calleeName = exprTypes.at(node->callee)->name;
                    bool isVecAccessor = bufferAccessors.contains(calleeName);
                    if (isVecAccessor
                            && bufferAccessors.at(calleeName)->mode == AccessorMode::BufferReference) {
                        isVecAccessor = false;
                    }

                    if (isVecAccessor) {
                        out << "(";
                    }
                    self(node->callee);
                    if (isVecAccessor) {
                        out << " + uvec2(0, ";
                    } else {
                        out << (node->paren->type == TokenType::RightParen ? "(" : "[");
                    }

                    for (auto cur = node->arguments.head; cur; cur = cur->next) {
                        auto curType = exprTypes.at(cur)->name;

                        if (imageAccessors.contains(curType)) {
                            // Unpack texture descriptor when passed as arguments
                            // TODO: Make sample/load/store operations member functions
                            //   of images
                            auto a = imageAccessors.at(curType);
                            if (a->mode == AccessorMode::SampledImage) {
                                out << "sampler" << a->dims << "D(" << a->name << "[";
                                self(cur);
                                out << ".x], Sampler[";
                                self(cur);
                                out << ".y])";
                            } else if (a->mode == AccessorMode::StorageImage) {
                                out << a->name << "[";
                                self(cur);
                                out << ".x]";
                            }
                        } else {
                            self(cur);
                        }

                        if (cur->next) {
                            out << ", ";
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
                    out << "(";
                    self(node->cond);
                    out << "\n ? (";
                    self(node->thenExpr);
                    out << ")\n : (";
                    self(node->elseExpr);
                    out << "))";
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