#include "VulkanRHI.hpp"

namespace nova
{
    Handle<Queue>::Impl::CommandPool* Handle<Queue>::Impl::AcquireCommandPool()
    {
        if (pools.empty()) {
            CommandPool* pool = new CommandPool{};
            vkh::Check(context->vkCreateCommandPool(context->device, PtrTo(VkCommandPoolCreateInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = family,
            }), context->alloc, &pool->command_pool));

            return pool;
        } else {
            auto pool = pools.back();
            pools.pop_back();

            return pool;
        }
    }

    void Handle<Queue>::Impl::DestroyCommandPools()
    {
        for (auto& pending : pending_command_lists) {
            delete pending.command_list.impl;
        }

        for (auto& pool : pools) {
            for (auto list : pool->available_command_lists) {
                delete list.impl;
            }

            context->vkDestroyCommandPool(context->device, pool->command_pool, context->alloc);
            delete pool;
        }
    }

    void Handle<Queue>::Impl::ReleaseCommandPoolForList(CommandList cmd)
    {
        if (cmd->recording) {
            pools.push_back(cmd->command_pool);
        }
        cmd->recording = false;
    }

    void Handle<Queue>::Impl::MoveCommandListToPending(CommandList cmd)
    {
        ReleaseCommandPoolForList(cmd);
        pending_command_lists.emplace_back(cmd, fence.PendingValue());
    }

    void Handle<Queue>::Impl::ClearPendingCommandLists()
    {
        auto current_value = fence.CurrentValue();
        while (!pending_command_lists.empty()) {
            auto& pending = pending_command_lists.front();
            if (current_value >= pending.fence_value) {
                auto* pool = pending.command_list->command_pool;
                vkh::Check(context->vkResetCommandBuffer(pending.command_list->buffer, 0));
                pool->available_command_lists.emplace_back(pending.command_list);
                pending_command_lists.pop_front();
            } else {
                break;
            }
        }
    }

    CommandList Queue::Begin() const
    {
        impl->ClearPendingCommandLists();

        CommandList cmd;

        auto* pool = impl->AcquireCommandPool();

        if (pool->available_command_lists.empty()) {
            cmd = { new CommandList::Impl };
            cmd->command_pool = pool;
            cmd->queue = *this;
            cmd->context = impl->context;

            vkh::Check(impl->context->vkAllocateCommandBuffers(impl->context->device, PtrTo(VkCommandBufferAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = pool->command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }), &cmd->buffer));
        } else {
            cmd = pool->available_command_lists.back();
            pool->available_command_lists.pop_back();
        }

        cmd->recording = true;

        cmd->using_shader_objects = impl->context->shader_objects;
        cmd->bound_graphics_pipeline = nullptr;
        cmd->shaders.clear();

        vkh::Check(impl->context->vkBeginCommandBuffer(cmd->buffer, PtrTo(VkCommandBufferBeginInfo {
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

    void CommandList::End() const
    {
        impl->queue->ReleaseCommandPoolForList(*this);
    }

    void CommandList::Discard() const
    {
        impl->queue->ReleaseCommandPoolForList(*this);
        vkh::Check(impl->context->vkResetCommandBuffer(impl->buffer, 0));
        impl->command_pool->available_command_lists.emplace_back(*this);
    }

// -----------------------------------------------------------------------------

    void CommandList::Barrier(PipelineStage src, PipelineStage dst) const
    {
        impl->context->vkCmdPipelineBarrier2(impl->buffer, PtrTo(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = PtrTo(VkMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = GetVulkanPipelineStage(src),
                .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = GetVulkanPipelineStage(dst),
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
            }),
        }));
    }
}