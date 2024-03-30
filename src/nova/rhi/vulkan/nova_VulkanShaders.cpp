#include "nova_VulkanRHI.hpp"

namespace nova
{
    Shader Shader::Create(HContext context, ShaderLang lang, ShaderStage stage, std::string entry, std::string_view filename, Span<std::string_view> fragments)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->stage = stage;

        std::vector<u32> spirv;
        switch (lang) {
            break;case ShaderLang::Glsl:
                spirv = Vulkan_CompileGlslToSpirv(stage, entry, filename, fragments);
            break;case ShaderLang::Hlsl:
                spirv = Vulkan_CompileHlslToSpirv(stage, entry, filename, fragments);
            break;case ShaderLang::Slang:
                spirv = Vulkan_CompileSlangToSpirv(stage, entry, filename, fragments);
            break;default:
                NOVA_THROW("Unknown shader language");
        }

        Shader shader{ impl };

        shader->id = context->GetUID();
        shader->entry = std::move(entry);

        bool generate_shader_object = true;
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
            break;case VK_SHADER_STAGE_RAYGEN_BIT_KHR:              generate_shader_object = false;
            break;case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:             generate_shader_object = false;
            break;case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:         generate_shader_object = false;
            break;case VK_SHADER_STAGE_MISS_BIT_KHR:                generate_shader_object = false;
            break;case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:        generate_shader_object = false;
            break;case VK_SHADER_STAGE_CALLABLE_BIT_KHR:            generate_shader_object = false;
            break;case VK_SHADER_STAGE_TASK_BIT_EXT:                next_stages = VK_SHADER_STAGE_MESH_BIT_EXT;
            break;case VK_SHADER_STAGE_MESH_BIT_EXT:                next_stages = VK_SHADER_STAGE_FRAGMENT_BIT;

            break;default: NOVA_THROW("Unknown stage: {}", int(shader->stage));
        }

        if (!shader->context->shader_objects)
            generate_shader_object = false;

        // TODO: remove shader module creation,
        //   store spirv and simply pass to pipelines

        vkh::Check(impl->context->vkCreateShaderModule(context->device, Temp(VkShaderModuleCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size() * sizeof(u32),
            .pCode = spirv.data(),
        }), context->alloc, &shader->handle));

        if (generate_shader_object) {
            vkh::Check(impl->context->vkCreateShadersEXT(context->device, 1, Temp(VkShaderCreateInfoEXT {
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
                .pPushConstantRanges = Temp(VkPushConstantRange {
                    .stageFlags = VK_SHADER_STAGE_ALL,
                    .size = context->properties.max_push_constant_size,
                }),
            }), context->alloc, &shader->shader));
        }

        return shader;
    }

    void Shader::Destroy()
    {
        if (!impl) {
            return;
        }

        impl->context->vkDestroyShaderModule(impl->context->device, impl->handle, impl->context->alloc);

        if (impl->shader) {
            impl->context->vkDestroyShaderEXT(impl->context->device, impl->shader, impl->context->alloc);
        }

        delete impl;
        impl = nullptr;
    }

    VkPipelineShaderStageCreateInfo Shader::Impl::GetStageInfo() const
    {
        return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VkShaderStageFlagBits(GetVulkanShaderStage(stage)),
            .module = handle,
            .pName = entry.c_str(),
        };
    }
}