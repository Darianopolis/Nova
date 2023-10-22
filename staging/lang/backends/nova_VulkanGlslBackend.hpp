#pragma once

#include <nova/lang/nova_Lang.hpp>

namespace nova
{
    struct VulkanGlslBackend
    {

        enum class AccessorMode
        {
            BufferReference,
            UniformBuffer,
            StorageBuffer,
            SampledImage,
            StorageImage,
        };

        struct BufferAccessor
        {
            Type*     element;
            AccessorMode mode;
            bool     readonly;

            std::string   name;
            Type* accessorType;
        };

        struct ImageAccessor
        {
            std::string_view format;
            i32                dims;
            AccessorMode       mode;

            std::string   name;
            Type* accessorType;
        };

    public:
        Parser* parser = nullptr;

        Type** currentFnType = nullptr;

        std::vector<HashMap<std::string_view, Type*>> scopes;
        HashMap<AstNode*, Type*>                   exprTypes;

        HashMap<std::string_view, BufferAccessor*> bufferAccessors;
        HashMap<std::string_view, ImageAccessor*>   imageAccessors;

    public:
        VulkanGlslBackend();

        BufferAccessor* RegisterBufferAccessor(Type* element, AccessorMode mode, bool readonly);
        ImageAccessor* RegisterImageAccessor(std::string_view format, i32 dims, AccessorMode mode);

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