#include "nova_Glsl.hpp"

namespace nova
{
    void GlslGenerator::Generate(std::ostream& out)
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
                    auto type = resolver->exprTypes.at(node)->name;
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
                    auto type = resolver->exprTypes.at(node->object)->name;
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