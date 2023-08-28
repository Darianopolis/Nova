#include "nova_VulkanRHI.hpp"

namespace nova
{
    void CommandList::PushConstants(u64 offset, u64 size, const void* data) const
    {
        vkCmdPushConstants(impl->buffer, impl->pool->context->pipelineLayout, VK_SHADER_STAGE_ALL, u32(offset), u32(size), data);
    }

// -----------------------------------------------------------------------------

    static
    constexpr std::array DynamicStates {
        VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
        VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
        VK_DYNAMIC_STATE_CULL_MODE,
        VK_DYNAMIC_STATE_FRONT_FACE,
        VK_DYNAMIC_STATE_LINE_WIDTH,
        VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
        VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
    };

    static
    VkPipeline GetGraphicsVertexInputStage(
        Context            context,
        const PipelineState& state)
    {
        auto key = GraphicsPipelineVertexInputStageKey {};

        // Set topology class
        switch (state.topology) {
        break;case Topology::Points:
            key.topology = Topology::Points;
        break;case Topology::Lines:
              case Topology::LineStrip:
            key.topology = Topology::Lines;
        break;case Topology::Triangles:
              case Topology::TriangleStrip:
              case Topology::TriangleFan:
            key.topology = Topology::Triangles;
        break;case Topology::Patches:
            key.topology = Topology::Patches;
        };

        auto pipeline = context->vertexInputStages[key];

        if (!pipeline) {
            auto start = std::chrono::steady_clock::now();
            VkCall(vkCreateGraphicsPipelines(context->device, context->pipelineCache,
                1, Temp(VkGraphicsPipelineCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = Temp(VkGraphicsPipelineLibraryCreateInfoEXT {
                        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                        .pNext = Temp(VkPipelineRenderingCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                        }),
                        .flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT,
                    }),
                    .flags = context->config.descriptorBuffers
                            ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                            : VkPipelineCreateFlags(0),
                    .pVertexInputState = Temp(VkPipelineVertexInputStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                    }),
                    .pInputAssemblyState = Temp(VkPipelineInputAssemblyStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                        .topology = GetVulkanTopology(state.topology),
                    }),
                    .pDynamicState = Temp(VkPipelineDynamicStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                        .dynamicStateCount = u32(DynamicStates.size()),
                        .pDynamicStates = DynamicStates.data(),
                    }),
                    .basePipelineIndex = -1,
                }), context->pAlloc, &pipeline));
            auto dur = std::chrono::steady_clock::now() - start;
            NOVA_LOG("Compiled new graphics vertex input    stage permutation in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));

            context->vertexInputStages[key] = pipeline;
        }

        return pipeline;
    }

    static
    VkPipeline GetGraphicsPreRasterizationStage(
        Context            context,
        Span<Shader>       shaders,
        const PipelineState& state)
    {
        auto key = GraphicsPipelinePreRasterizationStageKey {};

        // Set required fixed state
        key.polyMode = state.polyMode;

        // Set shaders and layout
        for (u32 i = 0; i < shaders.size(); ++i) {
            key.shaders[i] = shaders[i]->id;
        }

        auto pipeline = context->preRasterStages[key];

        if (!pipeline) {
            auto stages = NOVA_ALLOC_STACK(VkPipelineShaderStageCreateInfo, shaders.size());
            for (u32 i = 0; i < shaders.size(); ++i) {
                stages[i] = shaders[i]->GetStageInfo();
            }

            auto start = std::chrono::steady_clock::now();
            VkCall(vkCreateGraphicsPipelines(context->device, context->pipelineCache,
                1, Temp(VkGraphicsPipelineCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = Temp(VkPipelineRenderingCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                        .pNext = Temp(VkGraphicsPipelineLibraryCreateInfoEXT {
                            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                            .flags = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT,
                        }),
                    }),
                    .flags = context->config.descriptorBuffers
                            ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                            : VkPipelineCreateFlags(0),
                    .stageCount = u32(shaders.size()),
                    .pStages = stages,
                    .pViewportState = Temp(VkPipelineViewportStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                    }),
                    .pRasterizationState = Temp(VkPipelineRasterizationStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                        .polygonMode = GetVulkanPolygonMode(state.polyMode),
                    }),
                    .pDynamicState = Temp(VkPipelineDynamicStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                        .dynamicStateCount = u32(DynamicStates.size()),
                        .pDynamicStates = DynamicStates.data(),
                    }),
                    .layout = context->pipelineLayout,
                    .basePipelineIndex = -1,
                }), context->pAlloc, &pipeline));
            auto dur = std::chrono::steady_clock::now() - start;
            NOVA_LOG("Compiled new graphics pre-raster      stage permutation in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));

            context->preRasterStages[key] = pipeline;
        }

        return pipeline;
    }

    static
    VkPipeline GetGraphicsFragmentShaderStage(
        Context            context,
        Shader              shader,
        const PipelineState& state)
    {
        (void)state;

        auto key = GraphicsPipelineFragmentShaderStageKey {};
        key.shader = shader->id;

        auto pipeline = context->fragmentShaderStages[key];

        if (!pipeline) {
            auto start = std::chrono::steady_clock::now();
            VkCall(vkCreateGraphicsPipelines(context->device, context->pipelineCache,
                1, Temp(VkGraphicsPipelineCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = Temp(VkPipelineRenderingCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                        .pNext = Temp(VkGraphicsPipelineLibraryCreateInfoEXT {
                            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                            .flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT,
                        }),
                    }),
                    .flags = context->config.descriptorBuffers
                            ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                            : VkPipelineCreateFlags(0),
                    .stageCount = 1,
                    .pStages = Temp(shader->GetStageInfo()),
                    .pMultisampleState = Temp(VkPipelineMultisampleStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                    }),
                    .pDepthStencilState = Temp(VkPipelineDepthStencilStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                        .depthTestEnable = true,
                        .minDepthBounds = 0.f,
                        .maxDepthBounds = 1.f,
                    }),
                    .pDynamicState = Temp(VkPipelineDynamicStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                        .dynamicStateCount = u32(DynamicStates.size()),
                        .pDynamicStates = DynamicStates.data(),
                    }),
                    .layout = context->pipelineLayout,
                    .basePipelineIndex = -1,
                }), context->pAlloc, &pipeline));
            auto dur = std::chrono::steady_clock::now() - start;
            NOVA_LOG("Compiled new graphics fragment shader stage permutation in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));

            context->fragmentShaderStages[key] = pipeline;
        }

        return pipeline;
    }

    static
    VkPipeline GetGraphicsFragmentOutputStage(
        Context                    context,
        RenderingDescription renderingDesc,
        const PipelineState&         state)
    {
        auto key = GraphicsPipelineFragmentOutputStageKey {};

        // Set rendering info
        std::memcpy(key.colorAttachments.data(),
            renderingDesc.colorFormats.data(),
            renderingDesc.colorFormats.size() * sizeof(VkFormat));
        key.depthAttachment = renderingDesc.depthFormat;
        key.stencilAttachment = renderingDesc.stencilFormat;

        key.blendEnable = state.blendEnable;

        auto pipeline = context->fragmentOutputStages[key];

        if (!pipeline) {
            // Blend states

            auto attachBlendStates = NOVA_ALLOC_STACK(VkPipelineColorBlendAttachmentState, renderingDesc.colorFormats.size());

            for (u32 i = 0; i < renderingDesc.colorFormats.size(); ++i) {
                auto& attachState = attachBlendStates[i];

                attachState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                attachState.blendEnable = state.blendEnable;

                if (state.blendEnable) {
                    attachState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                    attachState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                    attachState.colorBlendOp = VK_BLEND_OP_ADD;
                    attachState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                    attachState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                    attachState.alphaBlendOp = VK_BLEND_OP_ADD;
                }
            }

            // Create

            auto vkFormats = NOVA_ALLOC_STACK(VkFormat, renderingDesc.colorFormats.size());
            for (u32 i = 0; i < renderingDesc.colorFormats.size(); ++i) {
                vkFormats[i] = GetVulkanFormat(renderingDesc.colorFormats[i]);
            }

            auto start = std::chrono::steady_clock::now();
            VkCall(vkCreateGraphicsPipelines(context->device, context->pipelineCache,
                1, Temp(VkGraphicsPipelineCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = Temp(VkPipelineRenderingCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                        .pNext = Temp(VkGraphicsPipelineLibraryCreateInfoEXT {
                            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                            .flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
                        }),
                        .colorAttachmentCount = u32(renderingDesc.colorFormats.size()),
                        .pColorAttachmentFormats = vkFormats,
                        .depthAttachmentFormat = GetVulkanFormat(renderingDesc.depthFormat),
                        .stencilAttachmentFormat = GetVulkanFormat(renderingDesc.stencilFormat),
                    }),
                    .flags = context->config.descriptorBuffers
                            ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                            : VkPipelineCreateFlags(0),
                    .pMultisampleState = Temp(VkPipelineMultisampleStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
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
                        .dynamicStateCount = u32(DynamicStates.size()),
                        .pDynamicStates = DynamicStates.data(),
                    }),
                    .basePipelineIndex = -1,
                }), context->pAlloc, &pipeline));
            auto dur = std::chrono::steady_clock::now() - start;
            NOVA_LOG("Compiled new graphics fragment output stage permutation in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));

            context->fragmentOutputStages[key] = pipeline;
        }

        return pipeline;
    }

    static
    VkPipeline GetGraphicsPipelineLibrarySet(
        Context            context,
        Span<VkPipeline> libraries)
    {
        auto key = GraphicsPipelineLibrarySetKey {};
        std::memcpy(key.stages.data(), libraries.data(), libraries.size() * sizeof(VkPipeline));

        auto pipeline = context->graphicsPipelineSets[key];

        if (!pipeline) {
            auto start = std::chrono::steady_clock::now();
            VkCall(vkCreateGraphicsPipelines(context->device, context->pipelineCache,
                1, Temp(VkGraphicsPipelineCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = Temp(VkPipelineLibraryCreateInfoKHR {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,
                        .libraryCount = u32(libraries.size()),
                        .pLibraries = libraries.data(),
                    }),
                    .layout = context->pipelineLayout,
                    .basePipelineIndex = -1,
                }), context->pAlloc, &pipeline));
            auto dur = std::chrono::steady_clock::now() - start;
            NOVA_LOG("Linked new graphics library set permutation in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));

            context->graphicsPipelineSets[key] = pipeline;
        }

        return pipeline;
    }

    void CommandList::SetGraphicsState(Span<HShader> _shaders, const PipelineState& pipelineState) const
    {
        auto start = std::chrono::steady_clock::now();

        auto context = impl->pool->context;

        // Viewport + Scissors

        {
            auto size = impl->renderingExtent;

            if (pipelineState.flipVertical) {
                vkCmdSetViewportWithCount(impl->buffer, 1, nova::Temp(VkViewport {
                    .y = f32(size.y),
                    .width = f32(size.x), .height = -f32(size.y),
                    .minDepth = 0.f, .maxDepth = 1.f,
                }));
            } else {
                vkCmdSetViewportWithCount(impl->buffer, 1, nova::Temp(VkViewport {
                    .width = f32(size.x), .height = f32(size.y),
                    .minDepth = 0.f, .maxDepth = 1.f,
                }));
            }
            vkCmdSetScissorWithCount(impl->buffer, 1, nova::Temp(VkRect2D {
                .extent = { size.x, size.y },
            }));
        }

        // Input Assembly

        vkCmdSetPrimitiveTopology(impl->buffer, GetVulkanTopology(pipelineState.topology));

        // Pre-rasterization (dynamic 2)

        vkCmdSetCullMode(impl->buffer, GetVulkanCullMode(pipelineState.cullMode));
        vkCmdSetFrontFace(impl->buffer, GetVulkanFrontFace(pipelineState.frontFace));
        vkCmdSetLineWidth(impl->buffer, pipelineState.lineWidth);

        // Depth + Stencil

        vkCmdSetDepthTestEnable(impl->buffer, pipelineState.depthEnable);
        vkCmdSetDepthWriteEnable(impl->buffer, pipelineState.depthWrite);
        vkCmdSetDepthCompareOp(impl->buffer, GetVulkanCompareOp(pipelineState.depthCompare));

        {
            // Separate shaders

            Shader fragmentShader = {};
            std::array<Shader, 4> preRasterStageShaders;
            u32 preRasterStageShaderIndex = 0;
            for (auto& shader : _shaders) {
                if (shader->stage == ShaderStage::Fragment) {
                    fragmentShader = shader;
                } else {
                    preRasterStageShaders[preRasterStageShaderIndex++] = shader;
                }
            }

            // Request pipeline

            auto vi = GetGraphicsVertexInputStage(context, pipelineState);
            auto pr = GetGraphicsPreRasterizationStage(impl->pool->context,
                { preRasterStageShaders.data(), preRasterStageShaderIndex }, pipelineState);
            auto fs = GetGraphicsFragmentShaderStage(context, fragmentShader, pipelineState);
            auto fo = GetGraphicsFragmentOutputStage(context, {
                    .colorFormats = impl->colorAttachmentsFormats,
                    .depthFormat = impl->depthAttachmentFormat,
                    .stencilFormat = impl->stencilAttachmentFormat,
                }, pipelineState);

            auto pipeline = GetGraphicsPipelineLibrarySet(context, { vi, pr, fs, fo });

            vkCmdBindPipeline(impl->buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        }

        rhi::stats::TimeSettingGraphicsState += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    }
}