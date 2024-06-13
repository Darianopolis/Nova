#include "nova_VulkanRHI.hpp"

#include <queue>

namespace nova
{
    void CommandList::PushConstants(const void* data, u64 size, u64 offset) const
    {
        impl->context->vkCmdPushConstants(impl->buffer,
            impl->context->global_heap.pipeline_layout, VK_SHADER_STAGE_ALL,
            u32(offset), u32(size), data);
    }

// -----------------------------------------------------------------------------

    static
    constexpr std::array DynamicStates = {
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

// -----------------------------------------------------------------------------

    static
    GraphicsPipelineKey GetGraphicsPipelineKey(CommandList cmd)
    {
        GraphicsPipelineKey key;

        // This needs to be explicit zeroed to handle padding bytes
        std::memset(&key, 0, sizeof(key));

        switch (cmd->topology) {
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
        }

        key.poly_mode = cmd->polygon_mode;

        for (u32 i = 0; i < cmd->shaders.size(); ++i) {
            key.shaders[i] = cmd->shaders[i]->id;
        }

        std::memcpy(
            key.color_attachments.data(),
            cmd->color_attachments_formats.data(),
            cmd->color_attachments_formats.size() * sizeof(VkFormat));
        key.depth_attachment = cmd->depth_attachment_format;
        key.stencil_attachment = cmd->stencil_attachment_format;

        key.blend_states = cmd->blend_states;

        key.view_mask = cmd->view_mask;

        return key;
    }

// -----------------------------------------------------------------------------
//                            Monolithic Pipelines
// -----------------------------------------------------------------------------

    static
    VkPipeline GetGraphicsMonolithPipeline(nova::CommandList cmd, const GraphicsPipelineKey& key)
    {
        NOVA_STACK_POINT();

        auto context = cmd->context;
        auto pipeline = context->graphics_pipeline_sets[key];
        if (pipeline) {
            return pipeline;
        }

        // Creating monolothic pipeline

        auto start = std::chrono::steady_clock::now();

        auto stages = NOVA_STACK_ALLOC(VkPipelineShaderStageCreateInfo, cmd->shaders.size());
        for (u32 i = 0; i < cmd->shaders.size(); ++i) {
            stages[i] = cmd->shaders[i]->GetStageInfo();
        }

        // Blend states
        // TODO: Deduplicate

        auto* attach_blend_states = NOVA_STACK_ALLOC(VkPipelineColorBlendAttachmentState, cmd->color_attachments_formats.size());

        for (u32 i = 0; i < cmd->color_attachments_formats.size(); ++i) {
            auto& attach_state = attach_blend_states[i];
            attach_state = {};

            attach_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            attach_state.blendEnable = cmd->blend_states[i];

            if (cmd->blend_states[i]) {
                attach_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                attach_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attach_state.colorBlendOp = VK_BLEND_OP_ADD;
                attach_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                attach_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attach_state.alphaBlendOp = VK_BLEND_OP_ADD;
            }
        }

        // Rendering info - formats

        auto vk_formats = NOVA_STACK_ALLOC(VkFormat, cmd->color_attachments_formats.size());
        for (u32 i = 0; i < cmd->color_attachments_formats.size(); ++i) {
            vk_formats[i] = GetVulkanFormat(cmd->color_attachments_formats[i]).vk_format;
        }

        vkh::Check(context->vkCreateGraphicsPipelines(context->device, context->pipeline_cache,
            1, PtrTo(VkGraphicsPipelineCreateInfo {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = PtrTo(VkPipelineRenderingCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    .viewMask = cmd->view_mask,
                    .colorAttachmentCount = u32(cmd->color_attachments_formats.size()),
                    .pColorAttachmentFormats = vk_formats,
                    .depthAttachmentFormat = GetVulkanFormat(cmd->depth_attachment_format).vk_format,
                    .stencilAttachmentFormat = GetVulkanFormat(cmd->stencil_attachment_format).vk_format,
                }),
                .flags = context->descriptor_buffers
                        ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                        : VkPipelineCreateFlags(0),
                .stageCount = u32(cmd->shaders.size()),
                .pStages = stages,
                .pVertexInputState = PtrTo(VkPipelineVertexInputStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                }),
                .pInputAssemblyState = PtrTo(VkPipelineInputAssemblyStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                    .topology = GetVulkanTopology(key.topology),
                }),
                .pViewportState = PtrTo(VkPipelineViewportStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                }),
                .pRasterizationState = PtrTo(VkPipelineRasterizationStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                    .polygonMode = GetVulkanPolygonMode(cmd->polygon_mode),
                }),
                .pMultisampleState = PtrTo(VkPipelineMultisampleStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                }),
                .pColorBlendState = PtrTo(VkPipelineColorBlendStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                    .logicOpEnable = VK_FALSE,
                    .logicOp = VK_LOGIC_OP_COPY,
                    .attachmentCount = u32(cmd->color_attachments_formats.size()),
                    .pAttachments = attach_blend_states,
                }),
                .pDynamicState = PtrTo(VkPipelineDynamicStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = u32(DynamicStates.size()),
                    .pDynamicStates = DynamicStates.data(),
                }),
                .layout = context->global_heap.pipeline_layout,
                .basePipelineIndex = -1,
            }), context->alloc, &pipeline));

        auto dur = std::chrono::steady_clock::now() - start;
        if (context->config.trace) {
            Log("Compiled new graphics pipeline in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));
        }

        context->graphics_pipeline_sets[key] = pipeline;
        return pipeline;
    }

// -----------------------------------------------------------------------------
//                         Graphics Pipeline Library
// -----------------------------------------------------------------------------

    static
    VkPipeline GetGraphicsVertexInputStage(CommandList cmd, const GraphicsPipelineKey& key)
    {
        auto context = cmd->context;

        // Extract relevant key parts and check for existing pipeline stage

        GraphicsPipelineVertexInputStageKey stage_key;
        std::memset(&stage_key, 0, sizeof(stage_key));
        stage_key.topology = key.topology;

        auto pipeline = context->vertex_input_stages[stage_key];
        if (pipeline) {
            return pipeline;
        }

        // Creating vertex input pipeline stage

        auto start = std::chrono::steady_clock::now();
        vkh::Check(context->vkCreateGraphicsPipelines(context->device, context->pipeline_cache,
            1, PtrTo(VkGraphicsPipelineCreateInfo {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = PtrTo(VkGraphicsPipelineLibraryCreateInfoEXT {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                    .pNext = PtrTo(VkPipelineRenderingCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    }),
                    .flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT,
                }),
                .flags = (context->descriptor_buffers
                        ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                        : VkPipelineCreateFlags(0))
                    | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR,
                .pVertexInputState = PtrTo(VkPipelineVertexInputStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                }),
                .pInputAssemblyState = PtrTo(VkPipelineInputAssemblyStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                    .topology = GetVulkanTopology(key.topology),
                }),
                .pMultisampleState = PtrTo(VkPipelineMultisampleStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                }),
                .pDepthStencilState = PtrTo(VkPipelineDepthStencilStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                }),
                .pDynamicState = PtrTo(VkPipelineDynamicStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = u32(DynamicStates.size()),
                    .pDynamicStates = DynamicStates.data(),
                }),
                .basePipelineIndex = -1,
            }), context->alloc, &pipeline));

        auto dur = std::chrono::steady_clock::now() - start;
        if (context->config.trace) {
            Log("Compiled new graphics vertex input    stage permutation in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));
        }

        context->vertex_input_stages[stage_key] = pipeline;
        return pipeline;
    }

    static
    VkPipeline GetGraphicsPreRasterizationStage(CommandList cmd, const GraphicsPipelineKey& key, Span<Shader> shaders)
    {
        NOVA_STACK_POINT();

        auto context = cmd->context;

        // Extract relevant key parts and check for existing pipeline stage

        GraphicsPipelinePreRasterizationStageKey stage_key;
        std::memset(&stage_key, 0, sizeof(stage_key));
        stage_key.poly_mode = key.poly_mode;
        stage_key.view_mask = key.view_mask;
        for (u32 i = 0; i < shaders.size(); ++i) {
            stage_key.shaders[i] = shaders[i]->id;
        }

        auto pipeline = context->preraster_stages[stage_key];
        if (pipeline) {
            return pipeline;
        }

        // Creating pre rasterization pipeline stage

        auto stages = NOVA_STACK_ALLOC(VkPipelineShaderStageCreateInfo, shaders.size());
        for (u32 i = 0; i < shaders.size(); ++i) {
            stages[i] = shaders[i]->GetStageInfo();
        }

        auto start = std::chrono::steady_clock::now();
        vkh::Check(context->vkCreateGraphicsPipelines(context->device, context->pipeline_cache,
            1, PtrTo(VkGraphicsPipelineCreateInfo {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = PtrTo(VkPipelineRenderingCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    .pNext = PtrTo(VkGraphicsPipelineLibraryCreateInfoEXT {
                        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                        .flags = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT,
                    }),
                }),
                .flags = (context->descriptor_buffers
                        ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                        : VkPipelineCreateFlags(0))
                    | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR,
                .stageCount = u32(shaders.size()),
                .pStages = stages,
                .pViewportState = PtrTo(VkPipelineViewportStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                }),
                .pRasterizationState = PtrTo(VkPipelineRasterizationStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                    .polygonMode = GetVulkanPolygonMode(cmd->polygon_mode),
                }),
                .pDynamicState = PtrTo(VkPipelineDynamicStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = u32(DynamicStates.size()),
                    .pDynamicStates = DynamicStates.data(),
                }),
                .layout = context->global_heap.pipeline_layout,
                .basePipelineIndex = -1,
            }), context->alloc, &pipeline));

        auto dur = std::chrono::steady_clock::now() - start;
        if (context->config.trace) {
            Log("Compiled new graphics pre-raster      stage permutation in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));
        }

        context->preraster_stages[stage_key] = pipeline;
        return pipeline;
    }

    static
    VkPipeline GetGraphicsFragmentShaderStage(CommandList cmd, const GraphicsPipelineKey& key, Shader shader)
    {
        auto context = cmd->context;

        // Extract relevant key parts and check for existing pipeline stage

        GraphicsPipelineFragmentShaderStageKey stage_key;
        std::memset(&stage_key, 0, sizeof(stage_key));
        stage_key.shader = shader->id;
        stage_key.view_mask = key.view_mask;

        auto pipeline = context->fragment_shader_stages[stage_key];
        if (pipeline) {
            return pipeline;
        }

        // Creating fragment shader pipeline stage

        auto start = std::chrono::steady_clock::now();
        vkh::Check(context->vkCreateGraphicsPipelines(context->device, context->pipeline_cache,
            1, PtrTo(VkGraphicsPipelineCreateInfo {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = PtrTo(VkPipelineRenderingCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    .pNext = PtrTo(VkGraphicsPipelineLibraryCreateInfoEXT {
                        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                        .flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT,
                    }),
                }),
                .flags = (context->descriptor_buffers
                        ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                        : VkPipelineCreateFlags(0))
                    | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR,
                .stageCount = 1,
                .pStages = PtrTo(shader->GetStageInfo()),
                .pMultisampleState = PtrTo(VkPipelineMultisampleStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                }),
                .pDepthStencilState = PtrTo(VkPipelineDepthStencilStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                }),
                .pDynamicState = PtrTo(VkPipelineDynamicStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = u32(DynamicStates.size()),
                    .pDynamicStates = DynamicStates.data(),
                }),
                .layout = context->global_heap.pipeline_layout,
                .basePipelineIndex = -1,
            }), context->alloc, &pipeline));

        auto dur = std::chrono::steady_clock::now() - start;
        if (context->config.trace) {
            Log("Compiled new graphics fragment shader stage permutation in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));
        }

        context->fragment_shader_stages[stage_key] = pipeline;
        return pipeline;
    }

    static
    VkPipeline GetGraphicsFragmentOutputStage(CommandList cmd, const GraphicsPipelineKey& key)
    {
        NOVA_STACK_POINT();

        auto context = cmd->context;

        // Extract relevant key parts and check for existing pipeline stage

        GraphicsPipelineFragmentOutputStageKey stage_key;
        std::memset(&stage_key, 0, sizeof(stage_key));
        stage_key.blend_states = key.blend_states;
        stage_key.color_attachments = key.color_attachments;
        stage_key.depth_attachment = key.depth_attachment;
        stage_key.stencil_attachment = key.stencil_attachment;
        stage_key.view_mask = key.view_mask;

        auto pipeline = context->fragment_output_stages[stage_key];
        if (pipeline) {
            return pipeline;
        }

        // Creating fragment output pipeline stage

        // Blend states
        // TODO: Deduplicate

        auto* attach_blend_states = NOVA_STACK_ALLOC(VkPipelineColorBlendAttachmentState, cmd->color_attachments_formats.size());

        for (u32 i = 0; i < cmd->color_attachments_formats.size(); ++i) {
            auto& attach_state = attach_blend_states[i];
            attach_state = {};

            attach_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            attach_state.blendEnable = cmd->blend_states[i];

            if (cmd->blend_states[i]) {
                attach_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                attach_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attach_state.colorBlendOp = VK_BLEND_OP_ADD;
                attach_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                attach_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attach_state.alphaBlendOp = VK_BLEND_OP_ADD;
            }
        }

        auto vk_formats = NOVA_STACK_ALLOC(VkFormat, cmd->color_attachments_formats.size());
        for (u32 i = 0; i < cmd->color_attachments_formats.size(); ++i) {
            vk_formats[i] = GetVulkanFormat(cmd->color_attachments_formats[i]).vk_format;
        }

        auto start = std::chrono::steady_clock::now();
        vkh::Check(cmd->context->vkCreateGraphicsPipelines(context->device, context->pipeline_cache,
            1, PtrTo(VkGraphicsPipelineCreateInfo {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = PtrTo(VkPipelineRenderingCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    .pNext = PtrTo(VkGraphicsPipelineLibraryCreateInfoEXT {
                        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                        .flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
                    }),
                    .colorAttachmentCount = u32(cmd->color_attachments_formats.size()),
                    .pColorAttachmentFormats = vk_formats,
                    .depthAttachmentFormat = GetVulkanFormat(cmd->depth_attachment_format).vk_format,
                    .stencilAttachmentFormat = GetVulkanFormat(cmd->stencil_attachment_format).vk_format,
                }),
                .flags = (context->descriptor_buffers
                        ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                        : VkPipelineCreateFlags(0))
                    | VK_PIPELINE_CREATE_LIBRARY_BIT_KHR,
                .pMultisampleState = PtrTo(VkPipelineMultisampleStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                }),
                .pColorBlendState = PtrTo(VkPipelineColorBlendStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                    .logicOpEnable = VK_FALSE,
                    .logicOp = VK_LOGIC_OP_COPY,
                    .attachmentCount = u32(cmd->color_attachments_formats.size()),
                    .pAttachments = attach_blend_states,
                }),
                .pDynamicState = PtrTo(VkPipelineDynamicStateCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = u32(DynamicStates.size()),
                    .pDynamicStates = DynamicStates.data(),
                }),
                .basePipelineIndex = -1,
            }), context->alloc, &pipeline));

        auto dur = std::chrono::steady_clock::now() - start;
        if (context->config.trace) {
            Log("Compiled new graphics fragment output stage permutation in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));
        }

        context->fragment_output_stages[stage_key] = pipeline;
        return pipeline;
    }

    static
    VkPipeline GetGraphicsPipelineLibrarySet(CommandList cmd, const GraphicsPipelineKey& key)
    {
        auto context = cmd->context;
        auto pipeline = context->graphics_pipeline_sets[key];
        if (pipeline) {
            return pipeline;
        }

        // Separate preraster/fragment shaders

        Shader fragment_shader = {};
        std::array<Shader, 4> preraster_stage_shaders;
        u32 preraster_stage_shader_index = 0;
        for (auto& shader : cmd->shaders) {
            if (shader->stage == ShaderStage::Fragment) {
                fragment_shader = shader;
            } else {
                preraster_stage_shaders[preraster_stage_shader_index++] = shader;
            }
        }

        // Get stages

        std::array<VkPipeline, 4> stages;
        stages[0] = GetGraphicsVertexInputStage(cmd, key);
        stages[1] = GetGraphicsPreRasterizationStage(cmd, key, { preraster_stage_shaders.data(), preraster_stage_shader_index });
        stages[2] = GetGraphicsFragmentShaderStage(cmd, key, fragment_shader);
        stages[3] = GetGraphicsFragmentOutputStage(cmd, key);

        // Link library

        auto start = std::chrono::steady_clock::now();
        vkh::Check(context->vkCreateGraphicsPipelines(context->device, context->pipeline_cache,
            1, PtrTo(VkGraphicsPipelineCreateInfo {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = PtrTo(VkPipelineLibraryCreateInfoKHR {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,
                    .libraryCount = 4,
                    .pLibraries = stages.data(),
                }),
                .flags = context->descriptor_buffers
                        ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                        : VkPipelineCreateFlags(0),
                .layout = context->global_heap.pipeline_layout,
                .basePipelineIndex = -1,
            }), context->alloc, &pipeline));

        auto dur = std::chrono::steady_clock::now() - start;
        if (context->config.trace) {
            Log("Linked new graphics library set permutation in {}",
                std::chrono::duration_cast<std::chrono::microseconds>(dur));
        }

        context->graphics_pipeline_sets[key] = pipeline;
        return pipeline;
    }

// -----------------------------------------------------------------------------

    void CommandList::Impl::EnsureGraphicsState()
    {
        if (!graphics_state_dirty || using_shader_objects) {
            return;
        }

        graphics_state_dirty = false;

        std::scoped_lock lock{ context->pipeline_cache_mutex };

        auto key = GetGraphicsPipelineKey({this});
        auto pipeline = context->graphics_pipeline_library
            ? GetGraphicsPipelineLibrarySet({this}, key)
            : GetGraphicsMonolithPipeline({this}, key);

        if (pipeline != bound_graphics_pipeline) {
            context->vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            bound_graphics_pipeline = pipeline;
        }
    }

// -----------------------------------------------------------------------------
//                            Shader Object API
// -----------------------------------------------------------------------------

    void CommandList::ResetGraphicsState() const
    {
        if (impl->using_shader_objects) {
            impl->context->vkCmdSetAlphaToCoverageEnableEXT(impl->buffer, false);
            impl->context->vkCmdSetSampleMaskEXT(impl->buffer, VK_SAMPLE_COUNT_1_BIT, nova::PtrTo<VkSampleMask>(0xFFFF'FFFF));
            impl->context->vkCmdSetRasterizationSamplesEXT(impl->buffer, VK_SAMPLE_COUNT_1_BIT);
            impl->context->vkCmdSetVertexInputEXT(impl->buffer, 0, nullptr, 0, nullptr);
        }

        impl->context->vkCmdSetRasterizerDiscardEnable(impl->buffer, false);
        impl->context->vkCmdSetPrimitiveRestartEnable(impl->buffer, false);

        // Stencil tests

        impl->context->vkCmdSetStencilTestEnable(impl->buffer, false);
        impl->context->vkCmdSetStencilOp(impl->buffer, VK_STENCIL_FACE_FRONT_AND_BACK,
            VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER);

        // Depth (extended)

        impl->context->vkCmdSetDepthBiasEnable(impl->buffer, false);
        impl->context->vkCmdSetDepthBoundsTestEnable(impl->buffer, false);
        impl->context->vkCmdSetDepthBounds(impl->buffer, 0.f, 1.f);

        SetPolygonState(Topology::Triangles, PolygonMode::Fill, 1.f);
        SetCullState(CullMode::None, FrontFace::CounterClockwise);
        SetDepthState(false, false, CompareOp::Always);
    }

    void CommandList::SetViewports(Span<Rect2I> viewports, bool copy_to_scissors) const
    {
        NOVA_STACK_POINT();

        auto vk_viewports = NOVA_STACK_ALLOC(VkViewport, viewports.size());
        for (u32 i = 0; i < viewports.size(); ++i) {
            vk_viewports[i] = VkViewport {
                .x = f32(viewports[i].offset.x),
                .y = f32(viewports[i].offset.y),
                .width = f32(viewports[i].extent.x),
                .height = f32(viewports[i].extent.y),
                .minDepth = 0.f,
                .maxDepth = 1.f
            };
        }
        impl->context->vkCmdSetViewportWithCount(impl->buffer, u32(viewports.size()), vk_viewports);

        if (copy_to_scissors) {
            SetScissors(viewports);
        }
    }

    void CommandList::SetScissors(Span<Rect2I> scissors) const
    {
        NOVA_STACK_POINT();

        auto vk_scissors = NOVA_STACK_ALLOC(VkRect2D, scissors.size());
        for (u32 i = 0; i < scissors.size(); ++i) {
            auto r = scissors[i];
            if (r.extent.x < 0) { r.offset.x -= (r.extent.x *= -1); }
            if (r.extent.y < 0) { r.offset.y -= (r.extent.y *= -1); }
            vk_scissors[i] = VkRect2D {
                .offset{     r.offset.x,      r.offset.y  },
                .extent{ u32(r.extent.x), u32(r.extent.y) },
            };
        }
        impl->context->vkCmdSetScissorWithCount(impl->buffer, u32(scissors.size()), vk_scissors);
    }

    void CommandList::SetPolygonState(
        Topology topology,
        PolygonMode polygon_mode,
        f32 line_width) const
    {
        impl->context->vkCmdSetPrimitiveTopology(impl->buffer, GetVulkanTopology(topology));
        if (impl->using_shader_objects) {
            impl->context->vkCmdSetPolygonModeEXT(impl->buffer, GetVulkanPolygonMode(polygon_mode));
        } else {
            impl->topology = topology;
            impl->polygon_mode = polygon_mode;
            impl->graphics_state_dirty = true;
        }
        impl->context->vkCmdSetLineWidth(impl->buffer, line_width);
    }

    void CommandList::SetCullState(
        CullMode cull_mode,
        FrontFace front_face) const
    {
        impl->context->vkCmdSetCullMode(impl->buffer, GetVulkanCullMode(cull_mode));
        impl->context->vkCmdSetFrontFace(impl->buffer, GetVulkanFrontFace(front_face));
    }

    void CommandList::SetDepthState(bool test_enable, bool write_enable, CompareOp compare_op) const
    {
        impl->context->vkCmdSetDepthTestEnable(impl->buffer, test_enable);
        impl->context->vkCmdSetDepthWriteEnable(impl->buffer, write_enable);
        impl->context->vkCmdSetDepthCompareOp(impl->buffer, GetVulkanCompareOp(compare_op));
    }

    void CommandList::SetBlendState(Span<bool> blends) const
    {
        NOVA_STACK_POINT();

        if (!impl->using_shader_objects) {
            for (u32 i = 0; i < blends.size(); ++i) {
                impl->blend_states.set(i, blends[i]);
            }
            impl->graphics_state_dirty = true;

            return;
        }

        std::priority_queue<int> queue;
        queue.push(0);

        auto count = u32(blends.size());

        auto components = NOVA_STACK_ALLOC(VkColorComponentFlags, count);
        auto blend_enable_bools = NOVA_STACK_ALLOC(VkBool32, count);
        auto blend_equations = NOVA_STACK_ALLOC(VkColorBlendEquationEXT, count);

        for (u32 i = 0; i < count; ++i) {
            components[i] = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blend_enable_bools[i] = blends[i];

            if (blends[i]) {
                blend_equations[i] = {
                    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                };
            } else {
                blend_equations[i] = {
                    .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                };
            }
        }

        impl->context->vkCmdSetColorBlendEnableEXT(impl->buffer, 0, count, blend_enable_bools);
        impl->context->vkCmdSetColorWriteMaskEXT(impl->buffer, 0, count, components);
        impl->context->vkCmdSetColorBlendEquationEXT(impl->buffer, 0, count, blend_equations);
    }

    void CommandList::BindShaders(Span<HShader> shaders) const
    {
        NOVA_STACK_POINT();

        if (!impl->using_shader_objects) {
            if (shaders.size() == 1 && shaders[0]->stage == ShaderStage::Compute) {

                // Compute

                auto key = ComputePipelineKey {};
                key.shader = shaders[0]->id;

                auto context = impl->context;

                auto pipeline = context->compute_pipelines[key];
                if (!pipeline) {
                    vkh::Check(impl->context->vkCreateComputePipelines(context->device, context->pipeline_cache, 1, PtrTo(VkComputePipelineCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                        .flags = context->descriptor_buffers
                                ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                                : VkPipelineCreateFlags(0),
                        .stage = shaders[0]->GetStageInfo(),
                        .layout = context->global_heap.pipeline_layout,
                        .basePipelineIndex = -1,
                    }), context->alloc, &pipeline));

                    context->compute_pipelines[key] = pipeline;
                }

                impl->context->vkCmdBindPipeline(impl->buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            } else {

                // Graphics

                impl->shaders.assign(shaders.begin(), shaders.end());
                impl->graphics_state_dirty = true;
            }

            return;
        }

        // Check for any shader changes

        bool any_changed = false;

        if (impl->shaders.size() != shaders.size()) {
            any_changed = true;
            impl->shaders.assign(shaders.begin(), shaders.end());
        } else {
            for (u32 i = 0; i < shaders.size(); ++i) {
                if (impl->shaders[i] != shaders[i]) {
                    any_changed = true;
                    impl->shaders[i] = shaders[i];
                }
            }
        }

        if (!any_changed) {
            return;
        }

        // Shaders changed, bind new shaders

        u32 count = u32(shaders.size());
        constexpr u32 MaxExtraSlots = 2;

        auto* stage_flags = NOVA_STACK_ALLOC(VkShaderStageFlagBits, count + MaxExtraSlots);
        auto* shader_objects = NOVA_STACK_ALLOC(VkShaderEXT, count + MaxExtraSlots);

        for (u32 i = 0; i < shaders.size(); ++i) {
            stage_flags[i] = VkShaderStageFlagBits(GetVulkanShaderStage(shaders[i]->stage));
            shader_objects[i] = shaders[i]->shader;

            if (impl->context->mesh_shading) {
                if (shaders[i]->stage == ShaderStage::Mesh) {
                    stage_flags[count] = VK_SHADER_STAGE_VERTEX_BIT;
                    shader_objects[count++] = VK_NULL_HANDLE;

                } else if (shaders[i]->stage == ShaderStage::Vertex) {
                    stage_flags[count] = VK_SHADER_STAGE_MESH_BIT_EXT;
                    shader_objects[count++] = VK_NULL_HANDLE;

                    stage_flags[count] = VK_SHADER_STAGE_TASK_BIT_EXT;
                    shader_objects[count++] = VK_NULL_HANDLE;
                }
            }
        }

        impl->context->vkCmdBindShadersEXT(impl->buffer, count, stage_flags, shader_objects);
    }
}
