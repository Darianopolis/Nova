#include "nova_VulkanRHI.hpp"

namespace nova
{
    void CommandList::SetComputeState(HShader shader) const
    {
        auto key = ComputePipelineKey {};
        key.shader = shader->id;

        auto context = impl->pool->context;

        auto pipeline = context->computePipelines[key];
        if (!pipeline) {
            VkCall(vkCreateComputePipelines(context->device, context->pipelineCache, 1, Temp(VkComputePipelineCreateInfo {
                .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .stage = shader->GetStageInfo(),
                .layout = context->pipelineLayout,
                .basePipelineIndex = -1,
            }), context->pAlloc, &pipeline));

            context->computePipelines[key] = pipeline;
        }

        vkCmdBindPipeline(impl->buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    }

    void CommandList::Dispatch(Vec3U groups) const
    {
        vkCmdDispatch(impl->buffer, groups.x, groups.y, groups.z);
    }
}