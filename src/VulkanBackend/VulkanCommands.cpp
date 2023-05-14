#include "VulkanBackend.hpp"

namespace pyr
{
    Ref<Commands> Context::CreateCommands()
    {
        Ref cmds = new Commands;
        cmds->context = this;
        cmds->queue = &graphics;

        VkCall(vkCreateCommandPool(device, Temp(VkCommandPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = cmds->queue->family,
        }), nullptr, &cmds->pool));

        return cmds;
    }

    Commands::~Commands()
    {
        vkDestroyCommandPool(context->device, pool, nullptr);
    }

    VkCommandBuffer Commands::Allocate()
    {
        VkCommandBuffer cmd;
        if (index >= buffers.size())
        {
            VkCommandBuffer& newCmd = buffers.emplace_back();
            VkCall(vkAllocateCommandBuffers(context->device, Temp(VkCommandBufferAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }), &newCmd));

            cmd = newCmd;
            index++;
        }
        else
        {
            cmd = buffers[index++];
        }

        VkCall(vkBeginCommandBuffer(cmd, Temp(VkCommandBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        })));

        return cmd;
    }

    void Commands::Flush()
    {
        if (queue && index)
        {
            auto bufferInfos = (VkCommandBufferSubmitInfo*)_alloca(index * sizeof(VkCommandBufferSubmitInfo));
            for (u32 i = 0; i < index; ++i)
            {
                bufferInfos[i] = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                    .commandBuffer = buffers[i],
                };

                VkCall(vkEndCommandBuffer(buffers[i]));
            }

            VkCall(vkQueueSubmit2(queue->handle, 1, Temp(VkSubmitInfo2 {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .commandBufferInfoCount = index,
                .pCommandBufferInfos = bufferInfos,
            }), VK_NULL_HANDLE));

            _freea(bufferInfos);

            VkCall(vkQueueWaitIdle(queue->handle));
        }

        index = 0;
        VkCall(vkResetCommandPool(context->device, pool, 0));
    }

    // void Context::Flush(VkCommandBuffer _cmd)
    // {
    //     if (!_cmd)
    //         _cmd = cmd;

    //     auto _pool = _cmd == cmd
    //         ? pool
    //         : transferPool;

    //     VkCall(vkEndCommandBuffer(_cmd));

    //     VkCall(vkQueueSubmit2(queue, 1, Temp(VkSubmitInfo2 {
    //         .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    //         .commandBufferInfoCount = 1,
    //         .pCommandBufferInfos = Temp(VkCommandBufferSubmitInfo {
    //             .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    //             .commandBuffer = _cmd,
    //         }),
    //     }), VK_NULL_HANDLE));

    //     VkCall(vkQueueWaitIdle(queue));
    //     VkCall(vkResetCommandPool(device, _pool, 0));
    //     VkCall(vkBeginCommandBuffer(_cmd, Temp(VkCommandBufferBeginInfo {
    //         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    //     })));
    // }
}