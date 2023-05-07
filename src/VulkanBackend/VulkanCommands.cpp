#include "VulkanBackend.hpp"

namespace pyr
{
    // Commands CreateCommands(Context* ctx, Queue* queue)
    // {
    //     Commands cmds;

    //     VkCall(vkCreateCommandPool(ctx->device, Temp(VkCommandPoolCreateInfo {
    //         .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    //         .queueFamilyIndex = queue->family,
    //     }), nullptr, &cmds.pool));

    //     return cmds;
    // }

    // VkCommandBuffer AllocCmdBuf(Context* ctx, Commands* cmds)
    // {
    //     VkCommandBuffer cmd;
    //     if (cmds->index >= cmds->buffers.size())
    //     {
    //         VkCommandBuffer& newCmd = cmds->buffers.emplace_back();
    //         VkCall(vkAllocateCommandBuffers(ctx->device, Temp(VkCommandBufferAllocateInfo {
    //             .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    //             .commandPool = cmds->pool,
    //             .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    //             .commandBufferCount = 1,
    //         }), &newCmd));

    //         cmd = newCmd;
    //         cmds->index++;
    //     }
    //     else
    //     {
    //         cmd = cmds->buffers[cmds->index++];
    //     }

    //     VkCall(vkBeginCommandBuffer(cmd, Temp(VkCommandBufferBeginInfo {
    //         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    //     })));

    //     return cmd;
    // }

    // void FlushCommands(Context* ctx, Commands* cmds, Queue* queue)
    // {
    //     if (queue && cmds->index)
    //     {
    //         auto bufferInfos = (VkCommandBufferSubmitInfo*)_alloca(cmds->index * sizeof(VkCommandBufferSubmitInfo));
    //         for (u32 i = 0; i < cmds->index; ++i)
    //         {
    //             bufferInfos[i] = {
    //                 .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    //                 .commandBuffer = cmds->buffers[i],
    //             };

    //             VkCall(vkEndCommandBuffer(cmds->buffers[i]));
    //         }

    //         VkCall(vkQueueSubmit2(queue->queue, 1, Temp(VkSubmitInfo2 {
    //             .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    //             .commandBufferInfoCount = cmds->index,
    //             .pCommandBufferInfos = bufferInfos,
    //         }), VK_NULL_HANDLE));

    //         _freea(bufferInfos);

    //         VkCall(vkQueueWaitIdle(queue->queue));
    //     }

    //     cmds->index = 0;
    //     VkCall(vkResetCommandPool(ctx->device, cmds->pool, 0));
    // }

    void Context::Flush(VkCommandBuffer _cmd)
    {
        if (!_cmd)
            _cmd = cmd;

        auto _pool = _cmd == cmd
            ? pool
            : transferPool;

        VkCall(vkEndCommandBuffer(_cmd));

        VkCall(vkQueueSubmit2(queue, 1, Temp(VkSubmitInfo2 {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = Temp(VkCommandBufferSubmitInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = _cmd,
            }),
        }), VK_NULL_HANDLE));

        VkCall(vkQueueWaitIdle(queue));
        VkCall(vkResetCommandPool(device, _pool, 0));
        VkCall(vkBeginCommandBuffer(_cmd, Temp(VkCommandBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        })));
    }
}