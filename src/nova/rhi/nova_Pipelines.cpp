#include "nova_RHI_Impl.hpp"

namespace nova
{
    static
    VkPipeline BuildGraphicsPipeline(
        ContextImpl* context,
        RenderingDescription renderingDesc,
        const PipelineState&         state,
        Span<Shader>               shaders,
        VkPipelineLayout            layout)
    {
        // Shaders

        auto stages = NOVA_ALLOC_STACK(VkPipelineShaderStageCreateInfo, shaders.size());
        for (u32 i = 0; i < shaders.size(); ++i)
            stages[i] = shaders[i].GetStageInfo();

        // Blend states

        auto attachBlendStates = NOVA_ALLOC_STACK(VkPipelineColorBlendAttachmentState, renderingDesc.colorFormats.size());

        for (u32 i = 0; i < renderingDesc.colorFormats.size(); ++i)
        {
            auto& attachState = attachBlendStates[i];

            attachState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            attachState.blendEnable = state.blendEnable;

            if (state.blendEnable)
            {
                attachState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                attachState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attachState.colorBlendOp = VK_BLEND_OP_ADD;
                attachState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                attachState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attachState.alphaBlendOp = VK_BLEND_OP_ADD;
            }
        }

        // Create

        VkPipeline pipeline;
        VkCall(vkCreateGraphicsPipelines(context->device, nullptr,
            1, Temp(VkGraphicsPipelineCreateInfo {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = Temp(VkPipelineRenderingCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    .colorAttachmentCount = u32(renderingDesc.colorFormats.size()),
                    .pColorAttachmentFormats = reinterpret_cast<const VkFormat*>(renderingDesc.colorFormats.data()),
                    .depthAttachmentFormat = VkFormat(renderingDesc.depthFormat),
                    .stencilAttachmentFormat = VkFormat(renderingDesc.stencilFormat),
                }),
                // .flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
                .stageCount = u32(shaders.size()),
                .pStages = stages,
                .pVertexInputState = Temp(VkPipelineVertexInputStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                }),
                .pInputAssemblyState = Temp(VkPipelineInputAssemblyStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                    .topology = VkPrimitiveTopology(state.topology),
                }),
                .pViewportState = Temp(VkPipelineViewportStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                    .viewportCount = 1,
                    .scissorCount = 1,
                }),
                .pRasterizationState = Temp(VkPipelineRasterizationStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                    .polygonMode = VkPolygonMode(state.polyMode),
                    .cullMode = VkCullModeFlags(state.cullMode),
                    .frontFace = VkFrontFace(state.frontFace),
                }),
                .pMultisampleState = Temp(VkPipelineMultisampleStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                }),
                .pDepthStencilState = Temp(VkPipelineDepthStencilStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                    .depthTestEnable = state.depthEnable,
                    .depthWriteEnable = state.depthWrite,
                    .depthCompareOp = VkCompareOp(state.depthCompare),
                    .minDepthBounds = 0.f,
                    .maxDepthBounds = 1.f,
                }),
                .pColorBlendState = Temp(VkPipelineColorBlendStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                    .logicOpEnable = VK_FALSE,
                    .logicOp = VK_LOGIC_OP_COPY,
                    .attachmentCount = u32(renderingDesc.colorFormats.size()),
                    .pAttachments = attachBlendStates,
                }),
                .pDynamicState = Temp(VkPipelineDynamicStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = 2,
                    .pDynamicStates = std::array {
                        VK_DYNAMIC_STATE_VIEWPORT,
                        VK_DYNAMIC_STATE_SCISSOR,
                    }.data(),
                }),
                .layout = layout,
                .renderPass = nullptr, // TODO: Non-dynamic rendering
                .subpass = 0,
                .basePipelineIndex = -1,
            }), context->pAlloc, &pipeline));

        return pipeline;
    }

    void CommandList::SetGraphicsState(Span<Shader> shaders, const PipelineState& state) const
    {
        // bool shaderObject = std::all_of(
        //     shaders.begin(), shaders.end(), [](auto& shader) { return shader->shader; });

        bool shaderObject = false;

        if (shaderObject)
        {
            // // Viewport + Scissors

            // {
            //     auto size = impl->state->renderingExtent;

            //     if (state.flipVertical)
            //     {
            //         vkCmdSetViewportWithCount(impl->buffer, 1, nova::Temp(VkViewport {
            //             .y = f32(size.y),
            //             .width = f32(size.x), .height = -f32(size.y),
            //             .minDepth = 0.f, .maxDepth = 1.f,
            //         }));
            //     }
            //     else
            //     {
            //         vkCmdSetViewportWithCount(impl->buffer, 1, nova::Temp(VkViewport {
            //             .width = f32(size.x), .height = f32(size.y),
            //             .minDepth = 0.f, .maxDepth = 1.f,
            //         }));
            //     }
            //     vkCmdSetScissorWithCount(impl->buffer, 1, nova::Temp(VkRect2D {
            //         .extent = { size.x, size.y },
            //     }));
            // }

            // // Shaders

            // BindShaders(shaders);

            // // Input Assembly

            // vkCmdSetPrimitiveTopology(impl->buffer, VkPrimitiveTopology(state.topology));

            // // Rasterization

            // vkCmdSetCullMode(impl->buffer, VkCullModeFlags(state.cullMode));
            // vkCmdSetFrontFace(impl->buffer, VkFrontFace(state.frontFace));
            // vkCmdSetPolygonModeEXT(impl->buffer, VkPolygonMode(state.polyMode));
            // vkCmdSetLineWidth(impl->buffer, state.lineWidth);

            // // Depth + Stencil

            // vkCmdSetDepthTestEnable(impl->buffer, state.depthEnable);
            // if (state.depthEnable)
            // {
            //     vkCmdSetDepthWriteEnable(impl->buffer, state.depthWrite);
            //     vkCmdSetDepthCompareOp(impl->buffer, VkCompareOp(state.depthCompare));
            // }

            // // Blending

            // {
            //     auto colorAttachmentCount = u32(impl->state->colorAttachmentsFormats.size());
            //     auto components = NOVA_ALLOC_STACK(VkColorComponentFlags, colorAttachmentCount);
            //     auto blendEnableBools = NOVA_ALLOC_STACK(VkBool32, colorAttachmentCount);
            //     auto blendEquations = state.blendEnable
            //         ? NOVA_ALLOC_STACK(VkColorBlendEquationEXT, colorAttachmentCount)
            //         : nullptr;

            //     for (u32 i = 0; i < colorAttachmentCount; ++i)
            //     {
            //         components[i] = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            //             | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            //         blendEnableBools[i] = state.blendEnable;

            //         if (state.blendEnable)
            //         {
            //             blendEquations[i] = {
            //                 .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            //                 .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            //                 .colorBlendOp = VK_BLEND_OP_ADD,
            //                 .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            //                 .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            //                 .alphaBlendOp = VK_BLEND_OP_ADD,
            //             };
            //         }
            //     }

            //     vkCmdSetColorWriteMaskEXT(impl->buffer, 0, colorAttachmentCount, components);
            //     vkCmdSetColorBlendEnableEXT(impl->buffer, 0, colorAttachmentCount, blendEnableBools);
            //     if (state.blendEnable)
            //         vkCmdSetColorBlendEquationEXT(impl->buffer, 0, colorAttachmentCount, blendEquations);
            // }
        }
        else
        {
            // Viewport + Scissors

            {
                auto size = impl->state->renderingExtent;

                if (state.flipVertical)
                {
                    vkCmdSetViewport(impl->buffer, 0, 1, nova::Temp(VkViewport {
                        .y = f32(size.y),
                        .width = f32(size.x), .height = -f32(size.y),
                        .minDepth = 0.f, .maxDepth = 1.f,
                    }));
                }
                else
                {
                    vkCmdSetViewport(impl->buffer, 0, 1, nova::Temp(VkViewport {
                        .width = f32(size.x), .height = f32(size.y),
                        .minDepth = 0.f, .maxDepth = 1.f,
                    }));
                }
                vkCmdSetScissor(impl->buffer, 0, 1, nova::Temp(VkRect2D {
                    .extent = { size.x, size.y },
                }));
            }

            // Build pipeline hash key

            PipelineStateKey key = {};
            std::memcpy(key.colorAttachments.data(),
                impl->state->colorAttachmentsFormats.data(),
                impl->state->colorAttachmentsFormats.size() * sizeof(VkFormat));
            key.depthAttachment = VkFormat(impl->state->depthAttachmentFormat);
            key.stencilAttachment = VkFormat(impl->state->stencilAttachmentFormat);
            key.state = state;
            key.state.flipVertical = false;
            for (u32 i = 0; i < shaders.size(); ++i)
                key.shaders[i] = shaders[i]->module;
            key.layout = shaders[0]->layout->layout;

            // Query pipeline and build missing permutations on demand

            auto& pipeline = impl->pool->context->pipelines[key];
            if (!pipeline)
            {
                auto start = std::chrono::steady_clock::now();
                pipeline = BuildGraphicsPipeline(impl->pool->context, {
                    .colorFormats = impl->state->colorAttachmentsFormats,
                    .depthFormat = impl->state->depthAttachmentFormat,
                    .stencilFormat = impl->state->stencilAttachmentFormat,
                }, state, shaders, key.layout);
                auto dur = std::chrono::steady_clock::now() - start;
                NOVA_LOG("Compiled new shader permutation in {}",
                    std::chrono::duration_cast<std::chrono::microseconds>(dur));
            }

            vkCmdBindPipeline(impl->buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        }
    }
}