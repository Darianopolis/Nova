#include "nova_VulkanRHI.hpp"

namespace nova
{
    void VulkanContext::Cmd_SetComputeState(CommandList cmd, PipelineLayout layout, Shader shader)
    {
        auto key = ComputePipelineKey {};
        key.shader = Get(shader).id;
        key.layout = Get(layout).id;

        auto pipeline = computePipelines[key];
        if (!pipeline)
        {
            VkCall(vkCreateComputePipelines(device, pipelineCache, 1, Temp(VkComputePipelineCreateInfo {
                .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .stage = Get(shader).GetStageInfo(),
                .layout = Get(layout).layout,
                .basePipelineIndex = -1,
            }), pAlloc, &pipeline));

            computePipelines[key] = pipeline;
        }

        vkCmdBindPipeline(Get(cmd).buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    }

    void VulkanContext::Cmd_Dispatch(CommandList cmd, Vec3U groups)
    {
        vkCmdDispatch(Get(cmd).buffer, groups.x, groups.y, groups.z);
    }
}