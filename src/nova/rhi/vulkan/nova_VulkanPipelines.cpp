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

        VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
        VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
        VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
        VK_DYNAMIC_STATE_STENCIL_OP,
        VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_BOUNDS,
    };

    static
    VkPipeline GetGraphicsVertexInputStage(Context context, Topology topology)
    {
        auto key = GraphicsPipelineVertexInputStageKey {};

        // Set topology class
        switch (topology) {
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
            vkh::Check(vkCreateGraphicsPipelines(context->device, context->pipelineCache,
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
                        .topology = GetVulkanTopology(key.topology),
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
    VkPipeline GetGraphicsPreRasterizationStage(Context context, Span<Shader> shaders, PolygonMode polygonMode)
    {
        auto key = GraphicsPipelinePreRasterizationStageKey {};

        // Set required fixed state
        key.polyMode = polygonMode;

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
            vkh::Check(vkCreateGraphicsPipelines(context->device, context->pipelineCache,
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
                        .polygonMode = GetVulkanPolygonMode(polygonMode),
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
    VkPipeline GetGraphicsFragmentShaderStage(Context context, Shader shader)
    {
        auto key = GraphicsPipelineFragmentShaderStageKey {};
        key.shader = shader->id;

        auto pipeline = context->fragmentShaderStages[key];

        if (!pipeline) {
            auto start = std::chrono::steady_clock::now();
            vkh::Check(vkCreateGraphicsPipelines(context->device, context->pipelineCache,
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
    VkPipeline GetGraphicsFragmentOutputStage(CommandList::Impl* impl, Context context, RenderingDescription renderingDesc)
    {
        auto key = GraphicsPipelineFragmentOutputStageKey {};

        // Set rendering info
        std::memcpy(key.colorAttachments.data(),
            renderingDesc.colorFormats.data(),
            renderingDesc.colorFormats.size() * sizeof(VkFormat));
        key.depthAttachment = renderingDesc.depthFormat;
        key.stencilAttachment = renderingDesc.stencilFormat;

        key.blendStates = impl->blendStates;

        auto pipeline = context->fragmentOutputStages[key];

        if (!pipeline) {
            // Blend states

            auto attachBlendStates = NOVA_ALLOC_STACK(VkPipelineColorBlendAttachmentState, renderingDesc.colorFormats.size());

            for (u32 i = 0; i < renderingDesc.colorFormats.size(); ++i) {
                auto& attachState = attachBlendStates[i];

                attachState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                attachState.blendEnable = impl->blendStates[i];

                if (impl->blendStates[i]) {
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
            vkh::Check(vkCreateGraphicsPipelines(context->device, context->pipelineCache,
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
    VkPipeline GetGraphicsPipelineLibrarySet(Context context, Span<VkPipeline> libraries)
    {
        auto key = GraphicsPipelineLibrarySetKey {};
        std::memcpy(key.stages.data(), libraries.data(), libraries.size() * sizeof(VkPipeline));

        auto pipeline = context->graphicsPipelineSets[key];

        if (!pipeline) {
            auto start = std::chrono::steady_clock::now();
            vkh::Check(vkCreateGraphicsPipelines(context->device, context->pipelineCache,
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

// -----------------------------------------------------------------------------

    void CommandList::Impl::EnsureGraphicsState()
    {
        if (!graphicsStateDirty || usingShaderObjects) {
            return;
        }

        graphicsStateDirty = false;

        // Separate shaders

        Shader fragmentShader = {};
        std::array<Shader, 4> preRasterStageShaders;
        u32 preRasterStageShaderIndex = 0;
        for (auto& shader : shaders) {
            if (shader->stage == ShaderStage::Fragment) {
                fragmentShader = shader;
            } else {
                preRasterStageShaders[preRasterStageShaderIndex++] = shader;
            }
        }

        // Request pipeline

        auto context = pool->context;

        auto vi = GetGraphicsVertexInputStage(context, topology);
        auto pr = GetGraphicsPreRasterizationStage(pool->context,
            { preRasterStageShaders.data(), preRasterStageShaderIndex }, polygonMode);
        auto fs = GetGraphicsFragmentShaderStage(context, fragmentShader);
        auto fo = GetGraphicsFragmentOutputStage(this, context, {
                .colorFormats = colorAttachmentsFormats,
                .depthFormat = depthAttachmentFormat,
                .stencilFormat = stencilAttachmentFormat,
            });

        auto pipeline = GetGraphicsPipelineLibrarySet(context, { vi, pr, fs, fo });

        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }

// -----------------------------------------------------------------------------
//                            Shader Object API
// -----------------------------------------------------------------------------

    void CommandList::ResetGraphicsState() const
    {
        if (impl->usingShaderObjects) {
            vkCmdSetAlphaToCoverageEnableEXT(impl->buffer, false);
            vkCmdSetSampleMaskEXT(impl->buffer, VK_SAMPLE_COUNT_1_BIT, nova::Temp<VkSampleMask>(0xFFFF'FFFF));
            vkCmdSetRasterizationSamplesEXT(impl->buffer, VK_SAMPLE_COUNT_1_BIT);
            vkCmdSetVertexInputEXT(impl->buffer, 0, nullptr, 0, nullptr);
        }

        vkCmdSetRasterizerDiscardEnable(impl->buffer, false);
        vkCmdSetPrimitiveRestartEnable(impl->buffer, false);

        // Stencil tests

        vkCmdSetStencilTestEnable(impl->buffer, false);
        vkCmdSetStencilOp(impl->buffer, VK_STENCIL_FACE_FRONT_AND_BACK,
            VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER);

        // Depth (extended)

        vkCmdSetDepthBiasEnable(impl->buffer, false);
        vkCmdSetDepthBoundsTestEnable(impl->buffer, false);
        vkCmdSetDepthBounds(impl->buffer, 0.f, 1.f);

        SetPolygonState(
            Topology::Triangles,
            PolygonMode::Fill,
            CullMode::None,
            FrontFace::CounterClockwise,
            1.f);
        SetDepthState(false, false, CompareOp::Always);
    }

    NOVA_NO_INLINE
    void CommandList::SetViewports(Span<Rect2I> viewports, bool copyToScissors) const
    {
        auto vkViewports = NOVA_ALLOC_STACK(VkViewport, viewports.size());
        for (u32 i = 0; i < viewports.size(); ++i) {
            vkViewports[i] = VkViewport {
                .x = f32(viewports[i].offset.x),
                .y = f32(viewports[i].offset.y),
                .width = f32(viewports[i].extent.x),
                .height = f32(viewports[i].extent.y),
                .minDepth = 0.f,
                .maxDepth = 1.f
            };
        }
        vkCmdSetViewportWithCount(impl->buffer, u32(viewports.size()), vkViewports);

        if (copyToScissors) {
            SetScissors(viewports);
        }
    }

    NOVA_NO_INLINE
    void CommandList::SetScissors(Span<Rect2I> scissors) const
    {
        auto vkScissors = NOVA_ALLOC_STACK(VkRect2D, scissors.size());
        for (u32 i = 0; i < scissors.size(); ++i) {
            auto r = scissors[i];
            if (r.extent.x < 0) { r.offset.x -= (r.extent.x *= -1); }
            if (r.extent.y < 0) { r.offset.y -= (r.extent.y *= -1); }
            vkScissors[i] = VkRect2D {
                .offset{     r.offset.x,      r.offset.y  },
                .extent{ u32(r.extent.x), u32(r.extent.y) },
            };
        }
        vkCmdSetScissorWithCount(impl->buffer, u32(scissors.size()), vkScissors);
    }

    void CommandList::SetPolygonState(
        Topology topology,
        PolygonMode polygonMode,
        CullMode cullMode,
        FrontFace frontFace,
        f32 lineWidth) const
    {
        vkCmdSetPrimitiveTopology(impl->buffer, GetVulkanTopology(topology));
        if (impl->usingShaderObjects) {
            vkCmdSetPolygonModeEXT(impl->buffer, GetVulkanPolygonMode(polygonMode));
        } else {
            impl->topology = topology;
            impl->polygonMode = polygonMode;
            impl->graphicsStateDirty = true;
        }
        vkCmdSetCullMode(impl->buffer, GetVulkanCullMode(cullMode));
        vkCmdSetFrontFace(impl->buffer, GetVulkanFrontFace(frontFace));
        vkCmdSetLineWidth(impl->buffer, lineWidth);
    }

    void CommandList::SetDepthState(bool testEnable, bool writeEnable, CompareOp compareOp) const
    {
        vkCmdSetDepthTestEnable(impl->buffer, testEnable);
        vkCmdSetDepthWriteEnable(impl->buffer, writeEnable);
        vkCmdSetDepthCompareOp(impl->buffer, GetVulkanCompareOp(compareOp));
    }

    NOVA_NO_INLINE
    void CommandList::SetBlendState(Span<bool> blends) const
    {
        if (!impl->usingShaderObjects) {
            for (u32 i = 0; i < blends.size(); ++i) {
                impl->blendStates.set(i, blends[i]);
            }
            impl->graphicsStateDirty = true;

            return;
        }

        auto count = u32(blends.size());
        bool anyBlend = std::any_of(blends.begin(), blends.end(), [](auto v) { return v; });

        auto components = NOVA_ALLOC_STACK(VkColorComponentFlags, count);
        auto blendEnableBools = NOVA_ALLOC_STACK(VkBool32, count);
        auto blendEquations = anyBlend
            ? NOVA_ALLOC_STACK(VkColorBlendEquationEXT, count)
            : nullptr;

        for (u32 i = 0; i < count; ++i) {
            components[i] = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendEnableBools[i] = blends[i];

            if (blends[i]) {
                blendEquations[i] = {
                    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                };
            } else if (anyBlend) {
                blendEquations[i] = {
                    .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                };
            }
        }

        vkCmdSetColorBlendEnableEXT(impl->buffer, 0, count, blendEnableBools);
        vkCmdSetColorWriteMaskEXT(impl->buffer, 0, count, components);
        if (anyBlend) {
            vkCmdSetColorBlendEquationEXT(impl->buffer, 0, count, blendEquations);
        }
    }

    NOVA_NO_INLINE
    void CommandList::BindShaders(Span<HShader> shaders) const
    {
        if (!impl->usingShaderObjects) {
            if (shaders.size() == 1 && shaders[0]->stage == ShaderStage::Compute) {

                // Compute

                auto key = ComputePipelineKey {};
                key.shader = shaders[0]->id;

                auto context = impl->pool->context;

                auto pipeline = context->computePipelines[key];
                if (!pipeline) {
                    vkh::Check(vkCreateComputePipelines(context->device, context->pipelineCache, 1, Temp(VkComputePipelineCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                        .stage = shaders[0]->GetStageInfo(),
                        .layout = context->pipelineLayout,
                        .basePipelineIndex = -1,
                    }), context->pAlloc, &pipeline));

                    context->computePipelines[key] = pipeline;
                }

                vkCmdBindPipeline(impl->buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            } else {

                // Graphics

                impl->shaders.assign(shaders.begin(), shaders.end());
                impl->graphicsStateDirty = true;
            }

            return;
        }

        u32 count = u32(shaders.size());
        constexpr u32 MaxExtraSlots = 2;

        auto stageFlags = NOVA_ALLOC_STACK(VkShaderStageFlagBits, count + MaxExtraSlots);
        auto shaderObjects = NOVA_ALLOC_STACK(VkShaderEXT, count + MaxExtraSlots);

        for (u32 i = 0; i < shaders.size(); ++i) {
            stageFlags[i] = VkShaderStageFlagBits(GetVulkanShaderStage(shaders[i]->stage));
            shaderObjects[i] = shaders[i]->shader;

            if (shaders[i]->stage == nova::ShaderStage::Mesh) {
                stageFlags[count] = VK_SHADER_STAGE_VERTEX_BIT;
                shaderObjects[count++] = VK_NULL_HANDLE;

            } else if (shaders[i]->stage == nova::ShaderStage::Vertex) {
                stageFlags[count] = VK_SHADER_STAGE_MESH_BIT_EXT;
                shaderObjects[count++] = VK_NULL_HANDLE;

                stageFlags[count] = VK_SHADER_STAGE_TASK_BIT_EXT;
                shaderObjects[count++] = VK_NULL_HANDLE;
            }
        }

        vkCmdBindShadersEXT(impl->buffer, count, stageFlags, shaderObjects);
    }
}