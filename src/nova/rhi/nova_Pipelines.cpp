#include "nova_RHI_Impl.hpp"

namespace nova
{
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
        Context context,
        const PipelineState& state)
    {
        auto key = GraphicsPipelineVertexInputStageKey {};

        // Set topology class
        switch (VkPrimitiveTopology(state.topology))
        {
        break;case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
            key.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

        break;case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
                case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
                case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
                case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
            key.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

        break;case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
                case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
                case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
                case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
                case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
            key.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        break;case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
            key.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        };

        auto pipeline = context->vertexInputStages[key];

        if (!pipeline)
        {
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
                        .topology = VkPrimitiveTopology(state.topology),
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
        const PipelineState& state,
        PipelineLayout      layout)
    {
        auto key = GraphicsPipelinePreRasterizationStageKey {};

        // Set required fixed state
        key.polyMode = VkPolygonMode(state.polyMode);

        // Set shaders and layout
        for (u32 i = 0; i < shaders.size(); ++i)
            key.shaders[i] = shaders[i]->id;
        key.layout = layout->id;

        auto pipeline = context->preRasterStages[key];

        if (!pipeline)
        {
            auto stages = NOVA_ALLOC_STACK(VkPipelineShaderStageCreateInfo, shaders.size());
            for (u32 i = 0; i < shaders.size(); ++i)
                stages[i] = shaders[i].GetStageInfo();

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
                        .polygonMode = VkPolygonMode(state.polyMode),
                    }),
                    .pDynamicState = Temp(VkPipelineDynamicStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                        .dynamicStateCount = u32(DynamicStates.size()),
                        .pDynamicStates = DynamicStates.data(),
                    }),
                    .layout = layout->layout,
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
        const PipelineState& state,
        PipelineLayout      layout)
    {
        (void)state;

        auto key = GraphicsPipelineFragmentShaderStageKey {};
        key.shader = shader->id;
        key.layout = layout->id;

        auto pipeline = context->fragmentShaderStages[key];

        if (!pipeline)
        {
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
                    .pStages = Temp(shader.GetStageInfo()),
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
                    .layout = layout->layout,
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
        key.depthAttachment = VkFormat(renderingDesc.depthFormat);
        key.stencilAttachment = VkFormat(renderingDesc.stencilFormat);

        key.blendEnable = state.blendEnable;

        auto pipeline = context->fragmentOutputStages[key];

        if (!pipeline)
        {

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
                        .pColorAttachmentFormats = reinterpret_cast<const VkFormat*>(renderingDesc.colorFormats.data()),
                        .depthAttachmentFormat = VkFormat(renderingDesc.depthFormat),
                        .stencilAttachmentFormat = VkFormat(renderingDesc.stencilFormat),
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
        Span<VkPipeline> libraries,
        VkPipelineLayout    layout)
    {
        auto key = GraphicsPipelineLibrarySetKey {};
        std::memcpy(key.stages.data(), libraries.data(), libraries.size() * sizeof(VkPipeline));

        auto pipeline = context->graphicsPipelineSets[key];

        if (!pipeline)
        {
            auto start = std::chrono::steady_clock::now();
            VkCall(vkCreateGraphicsPipelines(context->device, context->pipelineCache,
                1, Temp(VkGraphicsPipelineCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = Temp(VkPipelineLibraryCreateInfoKHR {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,
                        .libraryCount = u32(libraries.size()),
                        .pLibraries = libraries.data(),
                    }),
                    .layout = layout,
                    .basePipelineIndex = -1,
                }), context->pAlloc, &pipeline));
            auto dur = std::chrono::steady_clock::now() - start;
            NOVA_LOG("Linked new graphics library set permutation in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));

            context->graphicsPipelineSets[key] = pipeline;
        }

        return pipeline;
    }

    static
    VkPipeline GetGraphicsPipeline(
        Context                    context,
        RenderingDescription renderingDesc,
        const PipelineState&         state,
        Span<Shader>               shaders,
        PipelineLayout              layout)
    {
        GraphicsPipelineStateKey key = {};

        // Set rendering info
        std::memcpy(key.colorAttachments.data(),
            renderingDesc.colorFormats.data(),
            renderingDesc.colorFormats.size() * sizeof(VkFormat));
        key.depthAttachment = VkFormat(renderingDesc.depthFormat);
        key.stencilAttachment = VkFormat(renderingDesc.stencilFormat);

        // Set required fixed state
        key.polyMode = state.polyMode;
        key.blendEnable = state.blendEnable;

        // Set shaders and layout
        for (u32 i = 0; i < shaders.size(); ++i)
            key.shaders[i] = shaders[i]->id;
        key.layout = layout->id;

        // Query pipeline and build missing permutations on demand

        auto pipeline = context->pipelines[key];

        if (!pipeline)
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

            auto start = std::chrono::steady_clock::now();
            VkCall(vkCreateGraphicsPipelines(context->device, context->pipelineCache,
                1, Temp(VkGraphicsPipelineCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = Temp(VkPipelineRenderingCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                        .colorAttachmentCount = u32(renderingDesc.colorFormats.size()),
                        .pColorAttachmentFormats = reinterpret_cast<const VkFormat*>(renderingDesc.colorFormats.data()),
                        .depthAttachmentFormat = VkFormat(renderingDesc.depthFormat),
                        .stencilAttachmentFormat = VkFormat(renderingDesc.stencilFormat),
                    }),
                    .flags = context->config.descriptorBuffers
                        ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                        : VkPipelineCreateFlags(0),
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
                    }),
                    .pRasterizationState = Temp(VkPipelineRasterizationStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                        .polygonMode = VkPolygonMode(state.polyMode),
                    }),
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
                    .layout = layout->layout,
                    .renderPass = nullptr,
                    .subpass = 0,
                    .basePipelineIndex = -1,
                }), context->pAlloc, &pipeline));
            auto dur = std::chrono::steady_clock::now() - start;
            NOVA_LOG("Compiled new graphics pipeline permutation in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));

            context->pipelines[key] = pipeline;
        }

        return pipeline;
    }

    static
    VkShaderEXT GetShaderObject(
        Context       context,
        Shader         shader,
        PipelineLayout layout)
    {
        auto key = ShaderObjectKey {};
        key.shader = shader->id;
        key.layout = layout->id;
        auto shaderObject = context->shaderObjects[key];
        if (!shaderObject)
        {
            VkShaderStageFlags nextStages = {};
            switch (shader->stage)
            {
            break;case VK_SHADER_STAGE_VERTEX_BIT:
                nextStages = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
                            | VK_SHADER_STAGE_GEOMETRY_BIT
                            | VK_SHADER_STAGE_FRAGMENT_BIT;

            break;case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                nextStages = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

            break;case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                nextStages = VK_SHADER_STAGE_GEOMETRY_BIT
                            | VK_SHADER_STAGE_FRAGMENT_BIT;

            break;case VK_SHADER_STAGE_GEOMETRY_BIT:
                nextStages = VK_SHADER_STAGE_FRAGMENT_BIT;

            break;case VK_SHADER_STAGE_TASK_BIT_EXT:
                nextStages = VK_SHADER_STAGE_MESH_BIT_EXT;

            break;case VK_SHADER_STAGE_MESH_BIT_EXT:
                nextStages = VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            auto start = std::chrono::steady_clock::now();
            VkCall(vkCreateShadersEXT(context->device, 1, Temp(VkShaderCreateInfoEXT {
                .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
                .stage = shader->stage,
                .nextStage = nextStages,
                .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
                .codeSize = shader->spirv.size() * 4,
                .pCode = shader->spirv.data(),
                .pName = "main",
                .setLayoutCount = u32(layout->sets.size()),
                .pSetLayouts = layout->sets.data(),
                .pushConstantRangeCount = u32(layout->ranges.size()),
                .pPushConstantRanges = layout->ranges.data(),
            }), context->pAlloc, &shaderObject));
            auto dur = std::chrono::steady_clock::now() - start;
            NOVA_LOG("Compiled new shader object layout permutation in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));

            context->shaderObjects[key] = shaderObject;
        }

        return shaderObject;
    }

    void CommandList::SetGraphicsState(PipelineLayout layout, Span<Shader> shaders, const PipelineState& state) const
    {
        auto context = impl->pool->context;
        auto start = std::chrono::steady_clock::now();

        // Viewport + Scissors

        {
            auto size = impl->state->renderingExtent;

            if (state.flipVertical)
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

        // Input Assembly

        vkCmdSetPrimitiveTopology(impl->buffer, VkPrimitiveTopology(state.topology));

        // Pre-rasterization (dynamic 2)

        vkCmdSetCullMode(impl->buffer, VkCullModeFlags(state.cullMode));
        vkCmdSetFrontFace(impl->buffer, VkFrontFace(state.frontFace));
        vkCmdSetLineWidth(impl->buffer, state.lineWidth);

        // Depth + Stencil

        vkCmdSetDepthTestEnable(impl->buffer, state.depthEnable);
        vkCmdSetDepthWriteEnable(impl->buffer, state.depthWrite);
        vkCmdSetDepthCompareOp(impl->buffer, VkCompareOp(state.depthCompare));

        if (impl->pool->context->config.shaderObjects)
        {
            // Vertex input

            auto stageFlags = NOVA_ALLOC_STACK(VkShaderStageFlagBits, shaders.size());
            auto shaderObjects = NOVA_ALLOC_STACK(VkShaderEXT, shaders.size());
            for (u32 i = 0; i < shaders.size(); ++i)
            {
                stageFlags[i] = shaders[i]->stage;
                shaderObjects[i] = GetShaderObject(context, shaders[i], layout);
            }
            vkCmdBindShadersEXT(impl->buffer, u32(shaders.size()), stageFlags, shaderObjects);

            // Pre-rasterization (dynamic 3)

            vkCmdSetPolygonModeEXT(impl->buffer, VkPolygonMode(state.polyMode));

            // Fragment Output

            auto colorAttachmentCount = u32(impl->state->colorAttachmentsFormats.size());
            auto components = NOVA_ALLOC_STACK(VkColorComponentFlags, colorAttachmentCount);
            auto blendEnableBools = NOVA_ALLOC_STACK(VkBool32, colorAttachmentCount);
            auto blendEquations = state.blendEnable
                ? NOVA_ALLOC_STACK(VkColorBlendEquationEXT, colorAttachmentCount)
                : nullptr;

            for (u32 i = 0; i < colorAttachmentCount; ++i)
            {
                components[i] = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                blendEnableBools[i] = state.blendEnable;

                if (state.blendEnable)
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
            if (state.blendEnable)
                vkCmdSetColorBlendEquationEXT(impl->buffer, 0, colorAttachmentCount, blendEquations);
        }
        else
        {
            // Separate shaders

            Shader fragmentShader;
            std::array<Shader, 4> preRasterStageShaders;
            u32 preRasterStageShaderIndex = 0;
            for (auto& shader : shaders)
            {
                if (shader->stage == VK_SHADER_STAGE_FRAGMENT_BIT)
                    fragmentShader = shader;
                else
                    preRasterStageShaders[preRasterStageShaderIndex++] = shader;
            }

            // Request pipeline

            VkPipeline pipeline;
            if (context->usePipelineLibraries)
            {
                auto vi = GetGraphicsVertexInputStage(context, state);
                auto pr = GetGraphicsPreRasterizationStage(context,
                    { preRasterStageShaders.data(), preRasterStageShaderIndex }, state, layout);
                auto fs = GetGraphicsFragmentShaderStage(context, fragmentShader, state, layout);
                auto fo = GetGraphicsFragmentOutputStage(context, {
                        .colorFormats = impl->state->colorAttachmentsFormats,
                        .depthFormat = impl->state->depthAttachmentFormat,
                        .stencilFormat = impl->state->stencilAttachmentFormat,
                    }, state);

                pipeline = GetGraphicsPipelineLibrarySet(context, { vi, pr, fs, fo }, layout->layout);
            }
            else
            {
                pipeline = GetGraphicsPipeline(context, {
                        .colorFormats = impl->state->colorAttachmentsFormats,
                        .depthFormat = impl->state->depthAttachmentFormat,
                        .stencilFormat = impl->state->stencilAttachmentFormat,
                    }, state, shaders, layout);
            }

            vkCmdBindPipeline(impl->buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        }

        TimeSettingGraphicsState += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    }

// -----------------------------------------------------------------------------
//                             Compute shaders
// -----------------------------------------------------------------------------

    void CommandList::SetComputeState(PipelineLayout layout, Shader shader) const
    {
        auto context = impl->pool->context;

        auto key = ComputePipelineKey {};
        key.shader = shader->id;
        key.layout = layout->id;

        auto pipeline = context->computePipelines[key];
        if (!pipeline)
        {
            VkCall(vkCreateComputePipelines(context->device, context->pipelineCache, 1, Temp(VkComputePipelineCreateInfo {
                .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .stage = shader.GetStageInfo(),
                .layout = layout->layout,
                .basePipelineIndex = -1,
            }), context->pAlloc, &pipeline));

            context->computePipelines[key] = pipeline;
        }

        vkCmdBindPipeline(impl->buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    }

    void Context::CleanPipelines() const
    {
        std::scoped_lock lock { impl->pipelineMutex };

        auto eraseIfDeleted = [&](UID shaderID, VkPipeline pipeline) {
            if (shaderID != UID::Invalid && impl->deletedObjects.contains(shaderID))
            {
                vkDestroyPipeline(impl->device, pipeline, impl->pAlloc);
                impl->deletedPipelines.emplace(pipeline);
                return true;
            }

            return false;
        };

        // Clear traditional pipelines

        std::erase_if(impl->pipelines, [&](const auto& it) {
            for (auto sm : it.first.shaders)
            {
                if (eraseIfDeleted(sm, it.second))
                    return true;
            }

            return false;
        });

        // Clear shader objects

        std::erase_if(impl->shaderObjects, [&](const auto& it) {
            auto shaderID = it.first.shader;
            if (shaderID != UID::Invalid && impl->deletedObjects.contains(shaderID))
            {
                vkDestroyShaderEXT(impl->device, it.second, impl->pAlloc);
                return true;
            };

            return false;
        });

        // Clear compute pipelines

        std::erase_if(impl->computePipelines, [&](const auto& it) {
            return eraseIfDeleted(it.first.shader, it.second);
        });

        // Clear pipelines from pre-raster and vertex fragment shader stages that have dead shader links

        std::erase_if(impl->preRasterStages, [&](const auto& it) {
            for (auto sm : it.first.shaders)
            {
                if (eraseIfDeleted(sm, it.second))
                    return true;
            }

            return false;
        });

        std::erase_if(impl->fragmentShaderStages, [&](const auto& it) {
            return eraseIfDeleted(it.first.shader, it.second);
        });

        impl->deletedObjects.clear();

        // Clear pipeline library permutations with deleted stages

        std::erase_if(impl->graphicsPipelineSets, [&](auto it) {
            for (auto library : it.first.stages)
            {
                if (library && impl->deletedPipelines.contains(library))
                {
                    vkDestroyPipeline(impl->device, it.second, impl->pAlloc);
                    return true;
                }
            }

            return false;
        });

        impl->deletedPipelines.clear();
    }
}
