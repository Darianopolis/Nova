#pragma once

#include <nova/lang/nova_Lang.hpp>

namespace nova
{
    struct VulkanGlslBackend
    {
        Parser* parser = nullptr;

        Type** currentFnType = nullptr;

        std::vector<ankerl::unordered_dense::map<std::string_view, Type*>> scopes;
        ankerl::unordered_dense::map<AstNode*, Type*>                   exprTypes;

        enum class AccessorMode
        {
            BufferReference,
            UniformBuffer,
            StorageBuffer,
        };

        struct Accessor
        {
            Type*       element;
            AccessorMode   mode;
            bool       readonly;

            std::string    name;
            Type*  accessorType;
        };

        ankerl::unordered_dense::map<std::string_view, Accessor*> accessors;

    public:
        VulkanGlslBackend();

        Accessor* RegisterAccessor(Type* element, AccessorMode mode, bool readonly);

        void RegisterType(Type* type);
        void RegisterGlobal(std::string_view name, Type* type);

        Type* FindType(std::string_view name);
        Type* RecordAstType(AstNode* node, Type* type);

        void BeginScope();
        void EndScope();

        void Declare(Token* name);
        void Define(Token* name, Type* type);

        Type* ResolveLocal(Token* name);

        void Resolve();
        void PrintAst();

        void Generate(std::ostream& out);
    };
}