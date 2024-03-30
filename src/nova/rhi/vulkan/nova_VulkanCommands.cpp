#include "nova_VulkanRHI.hpp"

namespace nova
{
    void Handle<Queue>::Impl::InitCommands()
    {
        vkh::Check(context->vkCreateCommandPool(context->device, Temp(VkCommandPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = family,
        }), context->alloc, &pool.command_pool));
    }

    void Handle<Queue>::Impl::DestroyCommands()
    {
        for (auto& pending : pool.pending_command_lists) {
            delete pending.command_list.impl;
        }

        for (auto list : pool.available_command_lists) {
            delete list.impl;
        }

        context->vkDestroyCommandPool(context->device, pool.command_pool, context->alloc);
    }

    void Handle<Queue>::Impl::FreeCommandList(CommandList list)
    {
        pool.pending_command_lists.emplace_back(list, fence.PendingValue());
    }

    void Handle<Queue>::Impl::ClearPendingCommands()
    {
        auto current_value = fence.CurrentValue();
        while (!pool.pending_command_lists.empty()) {
            auto& pending = pool.pending_command_lists.front();
            if (current_value >= pending.fence_value) {
                vkh::Check(context->vkResetCommandBuffer(pending.command_list->buffer, 0));
                pool.available_command_lists.emplace_back(pending.command_list);
                pool.pending_command_lists.pop_front();
            } else {
                break;
            }
        }
    }

    CommandList Queue::Begin() const
    {
        CommandList cmd;

        if (impl->pool.available_command_lists.empty()) {
            cmd = { new CommandList::Impl };
            cmd->queue = *this;
            cmd->context = impl->context;

            vkh::Check(impl->context->vkAllocateCommandBuffers(impl->context->device, Temp(VkCommandBufferAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = impl->pool.command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }), &cmd->buffer));
        } else {
            cmd = impl->pool.available_command_lists.back();
            impl->pool.available_command_lists.pop_back();
        }

        cmd->using_shader_objects = impl->context->shader_objects;
        cmd->bound_graphics_pipeline = nullptr;
        cmd->shaders.clear();

        vkh::Check(impl->context->vkBeginCommandBuffer(cmd->buffer, Temp(VkCommandBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        })));

        if (impl->flags & VK_QUEUE_GRAPHICS_BIT) {
            cmd->context->global_heap.Bind(cmd, nova::BindPoint::Graphics);
        }
        if (impl->flags & VK_QUEUE_COMPUTE_BIT) {
            cmd->context->global_heap.Bind(cmd, nova::BindPoint::Compute);
            if (cmd->context.Config().ray_tracing) {
                cmd->context->global_heap.Bind(cmd, nova::BindPoint::RayTracing);
            }
        }

        return cmd;
    }

    void Queue::End(CommandList cmd) const
    {
        // TODO: With multiple pools, return pool held by list for reuse
    }

    void Queue::Release(CommandList cmd) const
    {
        vkh::Check(impl->context->vkResetCommandBuffer(cmd->buffer, 0));
        impl->pool.available_command_lists.emplace_back(cmd);
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