#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Files.hpp>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

#include <slang.h>
#include <slang-com-ptr.h>

namespace nova
{
    // TODO: Todo Includes

    std::vector<uint32_t> Vulkan_CompileSlangToSpirv(
            ShaderStage             /* stage */,
            std::string_view           entry,
            std::string_view     /* filename */,
            Span<std::string_view> fragments)
    {
        NOVA_STACK_POINT();

        std::string str;
        for (auto fragment : fragments) {
            str.append(fragment);
        }

        Slang::ComPtr<slang::IGlobalSession> slang_global_session;
        if (SLANG_FAILED(slang::createGlobalSession(slang_global_session.writeRef()))) {
            NOVA_THROW("slang failed creating global session");
        }

        slang::TargetDesc target_desc = {
            .format = SLANG_SPIRV,
            .profile = slang_global_session->findProfile("glsl460"),
            .flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY,
            .forceGLSLScalarBufferLayout = true,
        };

        // TODO: DXC Flags that need to copied over?
        // -spv-target-env=vulkan1.3
        // -HV 2021
        // -enable-16bit-types
        // -res-may-alias
        // -ffinite-math-only

        // slang::CompilerOptionEntry slang_entry;
        // slang_entry.name = slang::CompilerOptionName::Capability;
        // slang_entry.value.kind = slang::CompilerOptionValueKind::String;
        // slang_entry.value.stringValue0 = "spirv_1_5";

        slang::SessionDesc session_desc = {
            .targets = &target_desc,
            .targetCount = 1,
            .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
        };

        Slang::ComPtr<slang::ISession> session;
        if (SLANG_FAILED(slang_global_session->createSession(session_desc, session.writeRef()))) {
            NOVA_THROW("slang failed created local session");
        }

        slang::IModule* slang_module = nullptr;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            slang_module = session->loadModuleFromSourceString("shader", "generated", str.c_str(), diagnostics_blob.writeRef());
            if (diagnostics_blob) {
                NOVA_LOG("{}", (const char*)diagnostics_blob->getBufferPointer());
            }
            if (!slang_module) {
                NOVA_THROW("Expected slang_module, got none");
            }
        }

        Slang::ComPtr<slang::IEntryPoint> entry_point;
        if (SLANG_FAILED(slang_module->findEntryPointByName(NOVA_STACK_TO_CSTR(entry), entry_point.writeRef()))) {
            NOVA_THROW("slang failed to find entry point: {}", entry);
        }

        std::vector<slang::IComponentType*> component_types;
        component_types.push_back(slang_module);
        component_types.push_back(entry_point);

        Slang::ComPtr<slang::IComponentType> composed_program;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            auto result = session->createCompositeComponentType(
                component_types.data(),
                component_types.size(),
                composed_program.writeRef(),
                diagnostics_blob.writeRef());
            if (diagnostics_blob) {
                NOVA_LOG("{}", (const char*)diagnostics_blob->getBufferPointer());
            }
            if (SLANG_FAILED(result)) {
                NOVA_THROW("slang failed composing program");
            }
        }

        Slang::ComPtr<slang::IBlob> spirv_code;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            auto result = composed_program->getEntryPointCode(0, 0, spirv_code.writeRef(), diagnostics_blob.writeRef());
            if (diagnostics_blob) {
                NOVA_LOG("{}", (const char*)diagnostics_blob->getBufferPointer());
            }
            if (SLANG_FAILED(result)) {
                NOVA_THROW("slang failed composing program");
            }
        }

        std::vector<u32> spirv;
        spirv.resize(spirv_code->getBufferSize() / sizeof(u32));
        std::memcpy(spirv.data(), spirv_code->getBufferPointer(), spirv_code->getBufferSize());

        return spirv;
    }
}