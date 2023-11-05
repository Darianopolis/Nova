#include "nova_VulkanRHI.hpp"

namespace nova
{
    CommandPool CommandPool::Create(HContext context, HQueue queue)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->queue = queue;

        vkh::Check(context->vkCreateCommandPool(context->device, Temp(VkCommandPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = queue->family,
        }), context->pAlloc, &impl->pool));

        return { impl };
    }

    void CommandPool::Destroy()
    {
        if (!impl) {
            return;
        }

        for (auto& list : impl->lists) {
            delete list.impl;
        }

        impl->context->vkDestroyCommandPool(impl->context->device, impl->pool, impl->context->pAlloc);

        delete impl;
        impl = nullptr;
    }

    CommandList CommandPool::Begin() const
    {
        CommandList cmd;
        if (impl->index >= impl->lists.size()) {
            cmd = impl->lists.emplace_back(new CommandList::Impl);

            cmd->pool = *this;
            cmd->context = impl->context;
            vkh::Check(impl->context->vkAllocateCommandBuffers(impl->context->device, Temp(VkCommandBufferAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = impl->pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }), &cmd->buffer));
            impl->index++;
        } else {
            cmd = impl->lists[impl->index++];
        }

        cmd->usingShaderObjects = impl->context->shaderObjects;

        vkh::Check(impl->context->vkBeginCommandBuffer(cmd->buffer, Temp(VkCommandBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        })));

        if (impl->queue->flags & VK_QUEUE_GRAPHICS_BIT) {
            cmd->context->globalHeap.Bind(cmd, nova::BindPoint::Graphics);
        }
        if (impl->queue->flags & VK_QUEUE_COMPUTE_BIT) {
            cmd->context->globalHeap.Bind(cmd, nova::BindPoint::Compute);
            if (cmd->context.GetConfig().rayTracing) {
                cmd->context->globalHeap.Bind(cmd, nova::BindPoint::RayTracing);
            }
        }

        return cmd;
    }

    void CommandPool::Reset() const
    {
        impl->index = 0;
        vkh::Check(impl->context->vkResetCommandPool(impl->context->device, impl->pool, 0));
    }

// -----------------------------------------------------------------------------

    void CommandList::Barrier(PipelineStage src, PipelineStage dst) const
    {
        impl->context->vkCmdPipelineBarrier2(impl->buffer, Temp(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = Temp(VkMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = GetVulkanPipelineStage(src),
                .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = GetVulkanPipelineStage(dst),
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
            }),
        }));
    }
}