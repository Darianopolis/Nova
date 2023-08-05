#include "nova_VulkanRHI.hpp"

namespace nova
{
    void CommandList::SetComputeState(HPipelineLayout layout, HShader shader)
    {
        auto key = ComputePipelineKey {};
        key.shader = shader->id;
        key.layout = layout->id;

        auto context = pool->context;

        auto pipeline = context->computePipelines[key];
        if (!pipeline)
        {
            VkCall(vkCreateComputePipelines(context->device, context->pipelineCache, 1, Temp(VkComputePipelineCreateInfo {
                .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .stage = shader->GetStageInfo(),
                .layout = layout->layout,
                .basePipelineIndex = -1,
            }), context->pAlloc, &pipeline));

            context->computePipelines[key] = pipeline;
        }

        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    }

    void CommandList::Dispatch(Vec3U groups)
    {
        vkCmdDispatch(buffer, groups.x, groups.y, groups.z);
    }
}