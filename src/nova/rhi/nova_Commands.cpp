#include "nova_RHI.hpp"

namespace nova
{
    CommandPool* Context::CreateCommands()
    {
        auto cmds = new CommandPool;
        cmds->context = this;
        cmds->queue = graphics;

        VkCall(vkCreateCommandPool(device, Temp(VkCommandPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = cmds->queue->family,
        }), pAlloc, &cmds->pool));

        return cmds;
    }

    void Context::Destroy(CommandPool* pool)
    {
        vkDestroyCommandPool(device, pool->pool, pAlloc);

        for (auto& cmd : pool->buffers)
            delete cmd;

        delete pool;
    }

    CommandList* CommandPool::BeginPrimary(ResourceTracker* tracker)
    {
        CommandList* cmd;
        if (index >= buffers.size())
        {
            cmd = buffers.emplace_back(new CommandList);
            cmd->pool = this;
            VkCall(vkAllocateCommandBuffers(context->device, Temp(VkCommandBufferAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            }), &cmd->buffer));
            index++;
        }
        else
        {
            cmd = buffers[index++];
        }

        VkCall(vkBeginCommandBuffer(cmd->buffer, Temp(VkCommandBufferBeginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        })));

        cmd->tracker = tracker;

        return cmd;
    }

    CommandList* CommandPool::BeginSecondary(ResourceTracker* tracker, const RenderingDescription* renderingDescription)
    {
        CommandList* cmd;
        if (secondaryIndex >= secondaryBuffers.size())
        {
            cmd = secondaryBuffers.emplace_back(new CommandList);
            cmd->pool = this;
            VkCall(vkAllocateCommandBuffers(context->device, Temp(VkCommandBufferAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = pool,
                .level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
                .commandBufferCount = 1,
            }), &cmd->buffer));
            secondaryIndex++;
        }
        else
        {
            cmd = secondaryBuffers[secondaryIndex++];
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

        cmd->tracker = tracker;

        return cmd;
    }

    void CommandList::End()
    {
        VkCall(vkEndCommandBuffer(buffer));
    }

    void CommandPool::Reset()
    {
        index = 0;
        secondaryIndex = 0;
        VkCall(vkResetCommandPool(context->device, pool, 0));
    }

    NOVA_NO_INLINE
    void Queue::Submit(Span<CommandList*> commandLists, Span<Fence*> waits, Span<Fence*> signals)
    {
        auto bufferInfos = NOVA_ALLOC_STACK(VkCommandBufferSubmitInfo, commandLists.size());
        for (u32 i = 0; i < commandLists.size(); ++i)
        {
            auto cmd = commandLists.begin()[i];
            bufferInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = cmd->buffer,
            };

            cmd->End();
        }

        auto waitInfos = NOVA_ALLOC_STACK(VkSemaphoreSubmitInfo, waits.size());
        for (u32 i = 0; i < waits.size(); ++i)
        {
            auto wait = waits.begin()[i];
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
            auto signal = signals.begin()[i];
            signalInfos[i] = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = signal->semaphore,
                .value = ++signal->value,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            };
        }

        VkCall(vkQueueSubmit2(handle, 1, Temp(VkSubmitInfo2 {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = u32(waits.size()),
            .pWaitSemaphoreInfos = waitInfos,
            .commandBufferInfoCount = u32(commandLists.size()),
            .pCommandBufferInfos = bufferInfos,
            .signalSemaphoreInfoCount = u32(signals.size()),
            .pSignalSemaphoreInfos = signalInfos,
        }), nullptr));
    }

// -----------------------------------------------------------------------------

    void CommandList::SetViewport(Vec2U size, bool flipVertical)
    {
        if (flipVertical)
        {
            vkCmdSetViewportWithCount(buffer, 1, nova::Temp(VkViewport {
                .y = f32(size.y),
                .width = f32(size.x), .height = -f32(size.x),
                .minDepth = 0.f, .maxDepth = 1.f,
            }));
        }
        else
        {
            vkCmdSetViewportWithCount(buffer, 1, nova::Temp(VkViewport {
                .width = f32(size.x), .height = f32(size.y),
                .minDepth = 0.f, .maxDepth = 1.f,
            }));
        }
        vkCmdSetScissorWithCount(buffer, 1, nova::Temp(VkRect2D {
            .extent = { size.x, size.y },
        }));
    }

    void CommandList::SetTopology(VkPrimitiveTopology topology)
    {
        vkCmdSetPrimitiveTopology(buffer, topology);
    }

    NOVA_NO_INLINE
    void CommandList::SetBlendState(u32 colorAttachmentCount, bool blendEnable)
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

        vkCmdSetColorWriteMaskEXT(buffer, 0, colorAttachmentCount, components);
        vkCmdSetColorBlendEnableEXT(buffer, 0, colorAttachmentCount, blendEnableBools);
        if (blendEnable)
            vkCmdSetColorBlendEquationEXT(buffer, 0, colorAttachmentCount, blendEquations);
    }

    NOVA_NO_INLINE
    void CommandList::BeginRendering(Span<Image*> colorAttachments, Span<Vec4> clearColors, bool allowSecondary)
    {
        auto colorAttachmentInfos = NOVA_ALLOC_STACK(VkRenderingAttachmentInfo, colorAttachments.size());
        for (u32 i = 0; i < colorAttachments.size(); ++i)
        {
            auto image = colorAttachments.begin()[i];

            Transition(image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            colorAttachmentInfos[i] = VkRenderingAttachmentInfo {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = image->view,
                .imageLayout = image->layout,
                .loadOp = clearColors.size()
                    ? VK_ATTACHMENT_LOAD_OP_CLEAR
                    : VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            };

            if (clearColors.size())
            {
                auto& c = clearColors.begin()[i];
                colorAttachmentInfos[i].clearValue = {{{ c.r, c.g, c.b, c.a }}};
            }
        }

        vkCmdBeginRendering(buffer, Temp(VkRenderingInfo {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .flags = allowSecondary
                ? VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT
                : VkRenderingFlags(0),
            .renderArea = { {}, { colorAttachments.begin()[0]->extent.x, colorAttachments.begin()[0]->extent.y } },
            .layerCount = 1,
            .colorAttachmentCount = u32(colorAttachments.size()),
            .pColorAttachments = colorAttachmentInfos,
        }));
    }

    void CommandList::EndRendering()
    {
        vkCmdEndRendering(buffer);
    }

    NOVA_NO_INLINE
    void CommandList::BindShaders(Span<Shader*> shaders)
    {
        auto stageFlags = NOVA_ALLOC_STACK(VkShaderStageFlagBits, shaders.size());
        auto shaderObjects = NOVA_ALLOC_STACK(VkShaderEXT, shaders.size());
        for (u32 i = 0; i < shaders.size(); ++i)
        {
            stageFlags[i] = shaders.begin()[i]->stage;
            shaderObjects[i] = shaders.begin()[i]->shader;
        }

        vkCmdBindShadersEXT(buffer, shaders.size(), stageFlags, shaderObjects);
    }

    void CommandList::PushConstants(VkPipelineLayout layout, ShaderStage stages, u64 offset, u64 size, const void* data)
    {
        vkCmdPushConstants(buffer, layout, VkShaderStageFlags(stages), offset, size, data);
    }

    void CommandList::Draw(u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance)
    {
        vkCmdDraw(buffer, vertices, instances, firstVertex, firstInstance);
    }

    void CommandList::ClearAttachment(u32 attachment, Vec4 color, Vec2U size, Vec2I offset)
    {
        vkCmdClearAttachments(
            buffer, 1, nova::Temp(VkClearAttachment {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .colorAttachment = 0,
                .clearValue = {{{ color.r, color.g, color.b, color.a }}},
            }),
            1, nova::Temp(VkClearRect {
                .rect = { { offset.x, offset.y }, { size.x, size.y } },
                .baseArrayLayer = 0,
                .layerCount = 1,
            }));
    }

    void CommandList::ExecuteCommands(Span<CommandList*> commands)
    {
        auto buffers = NOVA_ALLOC_STACK(VkCommandBuffer, commands.size());
        for (u32 i = 0; i < commands.size(); ++i)
            buffers[i] = commands.begin()[i]->buffer;

        vkCmdExecuteCommands(buffer, commands.size(), buffers);
    }
}