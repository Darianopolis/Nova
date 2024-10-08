#include "VulkanRHI.hpp"

#include <nova/filesystem/VirtualFileSystem.hpp>

namespace nova
{
    namespace
    {
        struct ShaderCompilerEntry
        {
            ShaderLang                   lang;
            Vulkan_ShaderCompilerFuncPtr fptr;
        };

        struct ShaderCompilers
        {
            std::vector<ShaderCompilerEntry> entries;
        };

        ShaderCompilers& GetShaderCompilers()
        {
            static ShaderCompilers compilers;
            return compilers;
        }
    }

    std::monostate Vulkan_RegisterCompiler(ShaderLang lang, Vulkan_ShaderCompilerFuncPtr fptr)
    {
        Log("Registering compiler: Slang = {}", lang == ShaderLang::Slang);
        GetShaderCompilers().entries.emplace_back(lang, fptr);
        return {};
    }

    Shader Shader::Create(HContext context, ShaderLang lang, ShaderStage stage, std::string entry, StringView filename, Span<StringView> fragments)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->stage = stage;

        std::optional<std::vector<u32>> _spirv;
        std::string actual_entry = "main";

        {
            auto bytecode_resource_path = fs::path(filename).filename().replace_extension(".spv");

            if (auto data = vfs::LoadMaybe(bytecode_resource_path.string())) {
                nova::Log("Loading bytecode [{}]", bytecode_resource_path.string());

                std::vector<u32> code(data->size() / sizeof(u32));
                std::memcpy(code.data(), data->data(), data->size());
                _spirv = std::move(code);

                actual_entry = entry;
            }
        }

        if (!_spirv) {
            nova::Log("Could not find SPIR-V for shader, compiling dynamically...");

            for (auto& compiler : GetShaderCompilers().entries) {
                if (compiler.lang == lang) {
                    _spirv = compiler.fptr(stage, entry, filename, fragments);
                }
            }
        }

        if (!_spirv) {
            std::string_view lang_str;
            switch (lang) {
                break;case ShaderLang::Glsl:  lang_str = "GLSL";
                break;case ShaderLang::Slang: lang_str = "Slang";
            }
            NOVA_THROW("Cannot compile shader, no available compiler for {}", lang_str);
        }

        auto& spirv = *_spirv;

        Shader shader{ impl };

        shader->id = context->GetUID();
        shader->entry = std::move(actual_entry);

        bool is_rt_stage = false;
        VkShaderStageFlags next_stages = 0;

        switch (GetVulkanShaderStage(shader->stage)) {
            break;case VK_SHADER_STAGE_VERTEX_BIT:                  next_stages = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
                                                                        | VK_SHADER_STAGE_GEOMETRY_BIT
                                                                        | VK_SHADER_STAGE_FRAGMENT_BIT;
            break;case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    next_stages = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            break;case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: next_stages = VK_SHADER_STAGE_GEOMETRY_BIT
                                                                        | VK_SHADER_STAGE_FRAGMENT_BIT;
            break;case VK_SHADER_STAGE_GEOMETRY_BIT:                next_stages = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;case VK_SHADER_STAGE_FRAGMENT_BIT:                ;
            break;case VK_SHADER_STAGE_COMPUTE_BIT:                 ;
            break;case VK_SHADER_STAGE_RAYGEN_BIT_KHR:              is_rt_stage = true;
            break;case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:             is_rt_stage = true;
            break;case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:         is_rt_stage = true;
            break;case VK_SHADER_STAGE_MISS_BIT_KHR:                is_rt_stage = true;
            break;case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:        is_rt_stage = true;
            break;case VK_SHADER_STAGE_CALLABLE_BIT_KHR:            is_rt_stage = true;
            break;case VK_SHADER_STAGE_TASK_BIT_EXT:                next_stages = VK_SHADER_STAGE_MESH_BIT_EXT;
            break;case VK_SHADER_STAGE_MESH_BIT_EXT:                next_stages = VK_SHADER_STAGE_FRAGMENT_BIT;

            break;default: NOVA_UNREACHABLE();
        }

        if (shader->context->shader_objects && !is_rt_stage) {
            // shader objects
            vkh::Check(impl->context->vkCreateShadersEXT(context->device, 1, PtrTo(VkShaderCreateInfoEXT {
                .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
                .stage = VkShaderStageFlagBits(GetVulkanShaderStage(shader->stage)),
                .nextStage = next_stages,
                .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
                .codeSize = spirv.size() * sizeof(u32),
                .pCode = spirv.data(),
                .pName = shader->entry.c_str(),
                .setLayoutCount = 1,
                .pSetLayouts = &context->global_heap.heap_layout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = PtrTo(VkPushConstantRange {
                    .stageFlags = VK_SHADER_STAGE_ALL,
                    .size = context->properties.max_push_constant_size,
                }),
            }), context->alloc, &shader->shader));

        } else if (context->no_shader_modules) {
            // maintenance5 - pass create info to pipeline creation
            shader->code = std::move(spirv);
            shader->create_info = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = shader->code.size() * sizeof(u32),
                .pCode = shader->code.data(),
            };

        } else {
            // shader modules
            vkh::Check(impl->context->vkCreateShaderModule(context->device, PtrTo(VkShaderModuleCreateInfo {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = spirv.size() * sizeof(u32),
                .pCode = spirv.data(),
            }), context->alloc, &shader->handle));
        }

        return shader;
    }

    void Shader::Destroy()
    {
        if (!impl) {
            return;
        }

        if (impl->handle) {
            impl->context->vkDestroyShaderModule(impl->context->device, impl->handle, impl->context->alloc);
        }

        if (impl->shader) {
            impl->context->vkDestroyShaderEXT(impl->context->device, impl->shader, impl->context->alloc);
        }

        delete impl;
        impl = nullptr;
    }

    VkPipelineShaderStageCreateInfo Shader::Impl::GetStageInfo() const
    {
        VkPipelineShaderStageCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VkShaderStageFlagBits(GetVulkanShaderStage(stage)),
            .pName = entry.c_str(),
        };

        if (handle) info.module = handle;
        else        info.pNext  = &create_info;

        return info;
    }
}
