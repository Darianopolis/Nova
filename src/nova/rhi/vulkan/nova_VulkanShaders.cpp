#include "nova_VulkanRHI.hpp"

namespace nova
{
    Shader Shader::Create(HContext context, ShaderStage stage, std::string entry, Span<u32> spirv)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->stage = stage;

        Shader shader{ impl };

        shader->id = context->GetUID();
        shader->entry = std::move(entry);

        bool generateShaderObject = true;
        VkShaderStageFlags nextStages = 0;

        switch (GetVulkanShaderStage(shader->stage)) {
            break;case VK_SHADER_STAGE_VERTEX_BIT:                  nextStages = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
                                                                        | VK_SHADER_STAGE_GEOMETRY_BIT
                                                                        | VK_SHADER_STAGE_FRAGMENT_BIT;
            break;case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    nextStages = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            break;case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: nextStages = VK_SHADER_STAGE_GEOMETRY_BIT
                                                                        | VK_SHADER_STAGE_FRAGMENT_BIT;
            break;case VK_SHADER_STAGE_GEOMETRY_BIT:                nextStages = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;case VK_SHADER_STAGE_FRAGMENT_BIT:                ;
            break;case VK_SHADER_STAGE_COMPUTE_BIT:                 ;
            break;case VK_SHADER_STAGE_RAYGEN_BIT_KHR:              generateShaderObject = false;
            break;case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:             generateShaderObject = false;
            break;case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:         generateShaderObject = false;
            break;case VK_SHADER_STAGE_MISS_BIT_KHR:                generateShaderObject = false;
            break;case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:        generateShaderObject = false;
            break;case VK_SHADER_STAGE_CALLABLE_BIT_KHR:            generateShaderObject = false;
            break;case VK_SHADER_STAGE_TASK_BIT_EXT:                nextStages = VK_SHADER_STAGE_MESH_BIT_EXT;
            break;case VK_SHADER_STAGE_MESH_BIT_EXT:                nextStages = VK_SHADER_STAGE_FRAGMENT_BIT;

            break;default: NOVA_THROW("Unknown stage: {}", int(shader->stage));
        }

        if (!shader->context->shaderObjects)
            generateShaderObject = false;

        if (intptr_t(spirv.data()) % 4 != 0) {
            NOVA_THROW("SPIR-V must be 4 byte aligned!");
        }

        // TODO: remove shader module creation,
        //   store spirv and simply pass to pipelines

        vkh::Check(vkCreateShaderModule(context->device, Temp(VkShaderModuleCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size() * 4,
            .pCode = reinterpret_cast<const u32*>(spirv.data()),
        }), context->pAlloc, &shader->handle));

        if (generateShaderObject) {
            vkh::Check(vkCreateShadersEXT(context->device, 1, Temp(VkShaderCreateInfoEXT {
                .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
                .stage = VkShaderStageFlagBits(GetVulkanShaderStage(shader->stage)),
                .nextStage = nextStages,
                .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
                .codeSize = spirv.size() * 4,
                .pCode = spirv.data(),
                .pName = shader->entry.c_str(),
                .setLayoutCount = 1,
                .pSetLayouts = &context->heapLayout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = Temp(VkPushConstantRange {
                    .stageFlags = VK_SHADER_STAGE_ALL,
                    .size = 256,
                }),
            }), context->pAlloc, &shader->shader));
        }

        return shader;
    }

    void Shader::Destroy()
    {
        if (!impl) {
            return;
        }

        vkDestroyShaderModule(impl->context->device, impl->handle, impl->context->pAlloc);

        if (impl->shader) {
            vkDestroyShaderEXT(impl->context->device, impl->shader, impl->context->pAlloc);
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