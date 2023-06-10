#include "nova_RHI_Impl.hpp"

namespace nova
{
    NOVA_DEFINE_HANDLE_OPERATIONS(CommandPool)
    NOVA_DEFINE_HANDLE_OPERATIONS(CommandList)

    CommandPool::CommandPool(Context context, Queue queue)
        : ImplHandle(new CommandPoolImpl)
    {
        impl->context = context;
        impl->queue = queue;

        VkCall(vkCreateCommandPool(context->device, Temp(VkCommandPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = queue->family,
        }), context->pAlloc, &impl->pool));
    }

    CommandPoolImpl::~CommandPoolImpl()
    {
        if (pool)
            vkDestroyCommandPool(context->device, pool, context->pAlloc);
    }

    CommandList CommandPool::Begin(CommandState state) const
    {
        CommandList cmd;
        if (impl->index >= impl->buffers.size())
        {
            cmd.SetImpl(new CommandListImpl);
            impl->buffers.emplace_back(+cmd);

            cmd->pool = impl;
            VkCall(vkAllocateCommandBuffers(impl->context->device, Temp(VkCommandBufferAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = impl->pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }), &cmd->buffer));
            impl->index++;
        }
        else
        {
            cmd = impl->buffers[impl->index++];
        }

        VkCall(vkBeginCommandBuffer(cmd->buffer, Temp(VkCommandBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        })));

        cmd->state = state;

        return cmd;
    }

    CommandList CommandPool::BeginSecondary(CommandState state, OptRef<const RenderingDescription> renderingDescription) const
    {
        CommandList cmd;
        if (impl->secondaryIndex >= impl->secondaryBuffers.size())
        {
            cmd.SetImpl(new CommandListImpl);
            impl->secondaryBuffers.emplace_back(+cmd);

            VkCall(vkAllocateCommandBuffers(impl->context->device, Temp(VkCommandBufferAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = impl->pool,
                .level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
                .commandBufferCount = 1,
            }), &cmd->buffer));
            impl->secondaryIndex++;
        }
        else
        {
            cmd = impl->secondaryBuffers[impl->secondaryIndex++];
        }

        if (renderingDescription)
        {
            VkCall(vkBeginCommandBuffer(cmd->buffer, Temp(VkCommandBufferBeginInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
                .pInheritanceInfo = Temp(VkCommandBufferInheritanceInfo {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
                    .pNext = Temp(VkCommandBufferInheritanceRenderingInfo {
                        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
                        .colorAttachmentCount = u32(renderingDescription->colorFormats.size()),
                        .pColorAttachmentFormats = reinterpret_cast<const VkFormat*>(renderingDescription->colorFormats.begin()),
                        .depthAttachmentFormat = VkFormat(renderingDescription->depthFormat),
                        .stencilAttachmentFormat  = VkFormat(renderingDescription->stencilFormat),
                        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                    }),
                })
            })));
        }
        else
        {
            VkCall(vkBeginCommandBuffer(cmd->buffer, Temp(VkCommandBufferBeginInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pInheritanceInfo = Temp(VkCommandBufferInheritanceInfo {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
                }),
            })));
        }

        cmd->state = state;

        return cmd;
    }

    void CommandPool::Reset() const
    {
        impl->index = 0;
        impl->secondaryIndex = 0;
        VkCall(vkResetCommandPool(impl->context->device, impl->pool, 0));
    }

    void CommandList::End() const
    {
        VkCall(vkEndCommandBuffer(impl->buffer));
    }

    NOVA_NO_INLINE
    void Queue::Submit(Span<CommandList> commandLists, Span<Fence> waits, Span<Fence> signals) const
    {
        auto bufferInfos = NOVA_ALLOC_STACK(VkCommandBufferSubmitInfo, commandLists.size());
        for (u32 i = 0; i < commandLists.size(); ++i)
        {
            auto cmd = commandLists[i];
            bufferInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = cmd->buffer,
            };

            cmd.End();
        }

        auto waitInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, waits.size());
        for (u32 i = 0; i < waits.size(); ++i)
        {
            auto wait = waits[i];
            waitInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = wait->semaphore,
                .value = wait->value,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
        }

        auto signalInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, signals.size());
        for (u32 i = 0; i < signals.size(); ++i)
        {
            auto signal = signals[i];
            signalInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = signal->semaphore,
                .value = signal.Advance(),
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
        }

        auto start = std::chrono::steady_clock::now();
        VkCall(vkQueueSubmit2(impl->handle, 1, Temp(VkSubmitInfo2 {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = u32(waits.size()),
            .pWaitSemaphoreInfos = waitInfos,
            .commandBufferInfoCount = u32(commandLists.size()),
            .pCommandBufferInfos = bufferInfos,
            .signalSemaphoreInfoCount = u32(signals.size()),
            .pSignalSemaphoreInfos = signalInfos,
        }), nullptr));
        TimeSubmitting += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    }

// -----------------------------------------------------------------------------

    void CommandList::SetViewport(Vec2U size, bool flipVertical) const
    {
        if (flipVertical)
        {
            vkCmdSetViewportWithCount(impl->buffer, 1, nova::Temp(VkViewport {
                .y = f32(size.y),
                .width = f32(size.x), .height = -f32(size.y),
                .minDepth = 0.f, .maxDepth = 1.f,
            }));
        }
        else
        {
            vkCmdSetViewportWithCount(impl->buffer, 1, nova::Temp(VkViewport {
                .width = f32(size.x), .height = f32(size.y),
                .minDepth = 0.f, .maxDepth = 1.f,
            }));
        }
        vkCmdSetScissorWithCount(impl->buffer, 1, nova::Temp(VkRect2D {
            .extent = { size.x, size.y },
        }));
    }

    void CommandList::SetTopology(Topology topology) const
    {
        vkCmdSetPrimitiveTopology(impl->buffer, VkPrimitiveTopology(topology));
    }

    void CommandList::SetCullState(CullMode mode, FrontFace frontFace) const
    {
        vkCmdSetCullMode(impl->buffer, VkCullModeFlags(mode));
        vkCmdSetFrontFace(impl->buffer, VkFrontFace(frontFace));
    }

    void CommandList::SetPolyState(PolygonMode poly, f32 lineWidth) const
    {
        vkCmdSetPolygonModeEXT(impl->buffer, VkPolygonMode(poly));
        vkCmdSetLineWidth(impl->buffer, lineWidth);
    }

    NOVA_NO_INLINE
    void CommandList::SetBlendState(u32 colorAttachmentCount, bool blendEnable) const
    {
        auto components = NOVA_ALLOC_STACK(VkColorComponentFlags, colorAttachmentCount);
        auto blendEnableBools = NOVA_ALLOC_STACK(VkBool32, colorAttachmentCount);
        auto blendEquations = blendEnable
            ? NOVA_ALLOC_STACK(VkColorBlendEquationEXT, colorAttachmentCount)
            : nullptr;

        for (u32 i = 0; i < colorAttachmentCount; ++i)
        {
            components[i] = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendEnableBools[i] = blendEnable;

            if (blendEnable)
            {
                blendEquations[i] = {
                    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                };
            }
        }

        vkCmdSetColorWriteMaskEXT(impl->buffer, 0, colorAttachmentCount, components);
        vkCmdSetColorBlendEnableEXT(impl->buffer, 0, colorAttachmentCount, blendEnableBools);
        if (blendEnable)
            vkCmdSetColorBlendEquationEXT(impl->buffer, 0, colorAttachmentCount, blendEquations);
    }

    void CommandList::SetDepthState(bool enable, bool write, CompareOp compareOp) const
    {
        vkCmdSetDepthTestEnable(impl->buffer, enable);
        if (enable)
        {
            vkCmdSetDepthWriteEnable(impl->buffer, write);
            vkCmdSetDepthCompareOp(impl->buffer, VkCompareOp(compareOp));
        }
    }

    NOVA_NO_INLINE
    void CommandList::BeginRendering(Span<Texture> colorAttachments, Texture depthAttachment, Texture stencilAttachment, bool allowSecondary) const
    {
        impl->state->colorAttachmentsFormats.resize(colorAttachments.size());

        auto colorAttachmentInfos = NOVA_ALLOC_STACK(VkRenderingAttachmentInfo, colorAttachments.size());
        for (u32 i = 0; i < colorAttachments.size(); ++i)
        {
            auto texture = colorAttachments[i];

            Transition(texture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);

            colorAttachmentInfos[i] = VkRenderingAttachmentInfo {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = texture->view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };

            impl->state->colorAttachmentsFormats[i] = texture.GetFormat();
        }

        impl->state->renderingExtent = Vec2U(colorAttachments[0]->extent);

        VkRenderingInfo info {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .flags = allowSecondary
                ? VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT
                : VkRenderingFlags(0),
            .renderArea = { {}, { colorAttachments[0]->extent.x, colorAttachments[0]->extent.y } },
            .layerCount = 1,
            .colorAttachmentCount = u32(colorAttachments.size()),
            .pColorAttachments = colorAttachmentInfos,
        };

        VkRenderingAttachmentInfo depthInfo = {};
        VkRenderingAttachmentInfo stencilInfo = {};

        if (depthAttachment == stencilAttachment)
        {
            if (depthAttachment.IsValid())
            {
                Transition(depthAttachment,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR);

                depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depthInfo.imageView = depthAttachment->view;
                depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                info.pDepthAttachment = &depthInfo;
                info.pStencilAttachment = &depthInfo;

                impl->state->depthAttachmentFormat = depthAttachment.GetFormat();
                impl->state->stencilAttachmentFormat = stencilAttachment.GetFormat();
            }
        }
        else
        {
            if (depthAttachment.IsValid())
            {
                Transition(depthAttachment,
                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR);

                depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depthInfo.imageView = depthAttachment->view;
                depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                info.pDepthAttachment = &depthInfo;

                impl->state->depthAttachmentFormat = depthAttachment.GetFormat();
            }

            if (stencilAttachment.IsValid())
            {
                Transition(stencilAttachment,
                    VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR);

                stencilInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                stencilInfo.imageView = stencilAttachment->view;
                stencilInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                info.pStencilAttachment = &stencilInfo;

                impl->state->stencilAttachmentFormat = stencilAttachment.GetFormat();
            }
        }

        vkCmdBeginRendering(impl->buffer, &info);
    }

    void CommandList::EndRendering() const
    {
        vkCmdEndRendering(impl->buffer);
    }

    void CommandList::PushConstants(PipelineLayout layout, ShaderStage stages, u64 offset, u64 size, const void* data) const
    {
        vkCmdPushConstants(impl->buffer, layout->layout, VkShaderStageFlags(stages), u32(offset), u32(size), data);
    }

    void CommandList::Draw(u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance) const
    {
        vkCmdDraw(impl->buffer, vertices, instances, firstVertex, firstInstance);
    }

    void CommandList::DrawIndexed(u32 indices, u32 instances, u32 firstIndex, u32 vertexOffset, u32 firstInstance) const
    {
        vkCmdDrawIndexed(impl->buffer, indices, instances, firstIndex, vertexOffset, firstInstance);
    }

    void CommandList::ClearColor(u32 attachment, Vec4 color, Vec2U size, Vec2I offset) const
    {
        vkCmdClearAttachments(
            impl->buffer, 1, nova::Temp(VkClearAttachment {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .colorAttachment = attachment,
                .clearValue = {{{ color.r, color.g, color.b, color.a }}},
            }),
            1, nova::Temp(VkClearRect {
                .rect = { { offset.x, offset.y }, { size.x, size.y } },
                .baseArrayLayer = 0,
                .layerCount = 1,
            }));
    }

    void CommandList::ClearDepth(f32 depth, Vec2U size, Vec2I offset) const
    {
        vkCmdClearAttachments(
            impl->buffer, 1, nova::Temp(VkClearAttachment {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .clearValue = { .depthStencil = { .depth = depth } },
            }),
            1, nova::Temp(VkClearRect {
                .rect = { { offset.x, offset.y }, { size.x, size.y } },
                .baseArrayLayer = 0,
                .layerCount = 1,
            }));
    }

    void CommandList::ClearStencil(u32 value, Vec2U size, Vec2I offset) const
    {
        vkCmdClearAttachments(
            impl->buffer, 1, nova::Temp(VkClearAttachment {
                .aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT,
                .clearValue = { .depthStencil = { .stencil = value } },
            }),
            1, nova::Temp(VkClearRect {
                .rect = { { offset.x, offset.y }, { size.x, size.y } },
                .baseArrayLayer = 0,
                .layerCount = 1,
            }));
    }

    void CommandList::ExecuteCommands(Span<CommandList> commands) const
    {
        auto buffers = NOVA_ALLOC_STACK(VkCommandBuffer, commands.size());
        for (u32 i = 0; i < commands.size(); ++i)
            buffers[i] = commands[i]->buffer;

        vkCmdExecuteCommands(impl->buffer, u32(commands.size()), buffers);
    }

    void CommandList::Dispatch(Vec3U groups) const
    {
        vkCmdDispatch(impl->buffer, groups.x, groups.y, groups.z);
    }
}