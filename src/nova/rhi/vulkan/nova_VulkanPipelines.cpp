#include "nova_VulkanRHI.hpp"

#include <nova/core/nova_Stack.hpp>

namespace nova
{
    void CommandList::PushConstants(RawByteView data, u64 offset) const
    {
        impl->context->vkCmdPushConstants(impl->buffer,
            impl->context->global_heap.pipeline_layout, VK_SHADER_STAGE_ALL,
            u32(offset), u32(data.size), data.data);
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

        auto pipeline = context->vertex_input_stages[key];

        if (!pipeline) {
            auto start = std::chrono::steady_clock::now();
            vkh::Check(context->vkCreateGraphicsPipelines(context->device, context->pipeline_cache,
                1, Temp(VkGraphicsPipelineCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = Temp(VkGraphicsPipelineLibraryCreateInfoEXT {
                        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                        .pNext = Temp(VkPipelineRenderingCreateInfo {
                            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                        }),
                        .flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT,
                    }),
                    .flags = context->descriptor_buffers
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
                }), context->alloc, &pipeline));

            auto dur = std::chrono::steady_clock::now() - start;
            if (context->config.trace) {
                NOVA_LOG("Compiled new graphics vertex input    stage permutation in {}",
                    std::chrono::duration_cast<std::chrono::microseconds>(dur));
            }

            context->vertex_input_stages[key] = pipeline;
        }

        return pipeline;
    }

    static
    VkPipeline GetGraphicsPreRasterizationStage(Context context, Span<Shader> shaders, PolygonMode poly_mode)
    {
        NOVA_STACK_POINT();

        auto key = GraphicsPipelinePreRasterizationStageKey {};

        // Set required fixed state
        key.poly_mode = poly_mode;

        // Set shaders and layout
        for (u32 i = 0; i < shaders.size(); ++i) {
            key.shaders[i] = shaders[i]->id;
        }

        auto pipeline = context->preraster_stages[key];

        if (!pipeline) {
            auto stages = NOVA_STACK_ALLOC(VkPipelineShaderStageCreateInfo, shaders.size());
            for (u32 i = 0; i < shaders.size(); ++i) {
                stages[i] = shaders[i]->GetStageInfo();
            }

            auto start = std::chrono::steady_clock::now();
            vkh::Check(context->vkCreateGraphicsPipelines(context->device, context->pipeline_cache,
                1, Temp(VkGraphicsPipelineCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = Temp(VkPipelineRenderingCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                        .pNext = Temp(VkGraphicsPipelineLibraryCreateInfoEXT {
                            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                            .flags = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT,
                        }),
                    }),
                    .flags = context->descriptor_buffers
                            ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                            : VkPipelineCreateFlags(0),
                    .stageCount = u32(shaders.size()),
                    .pStages = stages,
                    .pViewportState = Temp(VkPipelineViewportStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                    }),
                    .pRasterizationState = Temp(VkPipelineRasterizationStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                        .polygonMode = GetVulkanPolygonMode(poly_mode),
                    }),
                    .pDynamicState = Temp(VkPipelineDynamicStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                        .dynamicStateCount = u32(DynamicStates.size()),
                        .pDynamicStates = DynamicStates.data(),
                    }),
                    .layout = context->global_heap.pipeline_layout,
                    .basePipelineIndex = -1,
                }), context->alloc, &pipeline));

            auto dur = std::chrono::steady_clock::now() - start;
            if (context->config.trace) {
                NOVA_LOG("Compiled new graphics pre-raster      stage permutation in {}",
                    std::chrono::duration_cast<std::chrono::microseconds>(dur));
            }

            context->preraster_stages[key] = pipeline;
        }

        return pipeline;
    }

    static
    VkPipeline GetGraphicsFragmentShaderStage(Context context, Shader shader)
    {
        auto key = GraphicsPipelineFragmentShaderStageKey {};
        key.shader = shader->id;

        auto pipeline = context->fragment_shader_stages[key];

        if (!pipeline) {
            auto start = std::chrono::steady_clock::now();
            vkh::Check(context->vkCreateGraphicsPipelines(context->device, context->pipeline_cache,
                1, Temp(VkGraphicsPipelineCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = Temp(VkPipelineRenderingCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                        .pNext = Temp(VkGraphicsPipelineLibraryCreateInfoEXT {
                            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                            .flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT,
                        }),
                    }),
                    .flags = context->descriptor_buffers
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
                    .layout = context->global_heap.pipeline_layout,
                    .basePipelineIndex = -1,
                }), context->alloc, &pipeline));

            auto dur = std::chrono::steady_clock::now() - start;
            if (context->config.trace) {
                NOVA_LOG("Compiled new graphics fragment shader stage permutation in {}",
                    std::chrono::duration_cast<std::chrono::microseconds>(dur));
            }

            context->fragment_shader_stages[key] = pipeline;
        }

        return pipeline;
    }

    static
    VkPipeline GetGraphicsFragmentOutputStage(CommandList::Impl* impl, Context context, RenderingDescription rendering_desc)
    {
        NOVA_STACK_POINT();

        auto key = GraphicsPipelineFragmentOutputStageKey {};

        // Set rendering info
        std::memcpy(key.color_attachments.data(),
            rendering_desc.color_formats.data(),
            rendering_desc.color_formats.size() * sizeof(VkFormat));
        key.depth_attachment = rendering_desc.depth_format;
        key.stencil_attachment = rendering_desc.stencil_format;

        key.blend_states = impl->blend_states;

        auto pipeline = context->fragment_output_stages[key];

        if (!pipeline) {

            // Blend states
            auto attach_blend_states = NOVA_STACK_ALLOC(VkPipelineColorBlendAttachmentState, rendering_desc.color_formats.size());

            for (u32 i = 0; i < rendering_desc.color_formats.size(); ++i) {
                auto& attach_state = attach_blend_states[i];

                attach_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                attach_state.blendEnable = impl->blend_states[i];

                if (impl->blend_states[i]) {
                    attach_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                    attach_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                    attach_state.colorBlendOp = VK_BLEND_OP_ADD;
                    attach_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                    attach_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                    attach_state.alphaBlendOp = VK_BLEND_OP_ADD;
                }
            }

            // Create

            auto vk_formats = NOVA_STACK_ALLOC(VkFormat, rendering_desc.color_formats.size());
            for (u32 i = 0; i < rendering_desc.color_formats.size(); ++i) {
                vk_formats[i] = GetVulkanFormat(rendering_desc.color_formats[i]).vk_format;
            }

            auto start = std::chrono::steady_clock::now();
            vkh::Check(impl->context->vkCreateGraphicsPipelines(context->device, context->pipeline_cache,
                1, Temp(VkGraphicsPipelineCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = Temp(VkPipelineRenderingCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                        .pNext = Temp(VkGraphicsPipelineLibraryCreateInfoEXT {
                            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
                            .flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
                        }),
                        .colorAttachmentCount = u32(rendering_desc.color_formats.size()),
                        .pColorAttachmentFormats = vk_formats,
                        .depthAttachmentFormat = GetVulkanFormat(rendering_desc.depth_format).vk_format,
                        .stencilAttachmentFormat = GetVulkanFormat(rendering_desc.stencil_format).vk_format,
                    }),
                    .flags = context->descriptor_buffers
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
                        .attachmentCount = u32(rendering_desc.color_formats.size()),
                        .pAttachments = attach_blend_states,
                    }),
                    .pDynamicState = Temp(VkPipelineDynamicStateCreateInfo {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                        .dynamicStateCount = u32(DynamicStates.size()),
                        .pDynamicStates = DynamicStates.data(),
                    }),
                    .basePipelineIndex = -1,
                }), context->alloc, &pipeline));

            auto dur = std::chrono::steady_clock::now() - start;
            if (context->config.trace) {
                NOVA_LOG("Compiled new graphics fragment output stage permutation in {}",
                    std::chrono::duration_cast<std::chrono::microseconds>(dur));
            }

            context->fragment_output_stages[key] = pipeline;
        }

        return pipeline;
    }

    static
    VkPipeline GetGraphicsPipelineLibrarySet(Context context, Span<VkPipeline> libraries)
    {
        auto key = GraphicsPipelineLibrarySetKey {};
        std::memcpy(key.stages.data(), libraries.data(), libraries.size() * sizeof(VkPipeline));

        auto pipeline = context->graphics_pipeline_sets[key];

        if (!pipeline) {
            auto start = std::chrono::steady_clock::now();
            vkh::Check(context->vkCreateGraphicsPipelines(context->device, context->pipeline_cache,
                1, Temp(VkGraphicsPipelineCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = Temp(VkPipelineLibraryCreateInfoKHR {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,
                        .libraryCount = u32(libraries.size()),
                        .pLibraries = libraries.data(),
                    }),
                    .flags = context->descriptor_buffers
                            ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                            : VkPipelineCreateFlags(0),
                    .layout = context->global_heap.pipeline_layout,
                    .basePipelineIndex = -1,
                }), context->alloc, &pipeline));

            auto dur = std::chrono::steady_clock::now() - start;
            if (context->config.trace) {
                NOVA_LOG("Linked new graphics library set permutation in {}",
                    std::chrono::duration_cast<std::chrono::microseconds>(dur));
            }

            context->graphics_pipeline_sets[key] = pipeline;
        }

        return pipeline;
    }

// -----------------------------------------------------------------------------

    void CommandList::Impl::EnsureGraphicsState()
    {
        if (!graphics_state_dirty || using_shader_objects) {
            return;
        }

        graphics_state_dirty = false;

        // Separate shaders

        Shader fragment_shader = {};
        std::array<Shader, 4> preraster_stage_shaders;
        u32 preraster_stage_shader_index = 0;
        for (auto& shader : shaders) {
            if (shader->stage == ShaderStage::Fragment) {
                fragment_shader = shader;
            } else {
                preraster_stage_shaders[preraster_stage_shader_index++] = shader;
            }
        }

        // Request pipeline

        auto vi = GetGraphicsVertexInputStage(context, topology);
        auto pr = GetGraphicsPreRasterizationStage(pool->context,
            { preraster_stage_shaders.data(), preraster_stage_shader_index }, polygon_mode);
        auto fs = GetGraphicsFragmentShaderStage(context, fragment_shader);
        auto fo = GetGraphicsFragmentOutputStage(this, context, {
                .color_formats = color_attachments_formats,
                .depth_format = depth_attachment_format,
                .stencil_format = stencil_attachment_format,
            });

        auto pipeline = GetGraphicsPipelineLibrarySet(context, { vi, pr, fs, fo });

        context->vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }

// -----------------------------------------------------------------------------
//                            Shader Object API
// -----------------------------------------------------------------------------

    void CommandList::ResetGraphicsState() const
    {
        if (impl->using_shader_objects) {
            impl->context->vkCmdSetAlphaToCoverageEnableEXT(impl->buffer, false);
            impl->context->vkCmdSetSampleMaskEXT(impl->buffer, VK_SAMPLE_COUNT_1_BIT, nova::Temp<VkSampleMask>(0xFFFF'FFFF));
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

        auto count = u32(blends.size());
        bool any_blend = std::any_of(blends.begin(), blends.end(), [](auto v) { return v; });

        auto components = NOVA_STACK_ALLOC(VkColorComponentFlags, count);
        auto blend_enable_bools = NOVA_STACK_ALLOC(VkBool32, count);
        auto blend_equations = any_blend
            ? NOVA_STACK_ALLOC(VkColorBlendEquationEXT, count)
            : nullptr;

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
            } else if (any_blend) {
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
        if (any_blend) {
            impl->context->vkCmdSetColorBlendEquationEXT(impl->buffer, 0, count, blend_equations);
        }
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
                    vkh::Check(impl->context->vkCreateComputePipelines(context->device, context->pipeline_cache, 1, Temp(VkComputePipelineCreateInfo {
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

        u32 count = u32(shaders.size());
        constexpr u32 MaxExtraSlots = 2;

        auto stage_flags = NOVA_STACK_ALLOC(VkShaderStageFlagBits, count + MaxExtraSlots);
        auto shader_objects = NOVA_STACK_ALLOC(VkShaderEXT, count + MaxExtraSlots);

        for (u32 i = 0; i < shaders.size(); ++i) {
            stage_flags[i] = VkShaderStageFlagBits(GetVulkanShaderStage(shaders[i]->stage));
            shader_objects[i] = shaders[i]->shader;

            if (impl->context->config.mesh_shading) {
                if (shaders[i]->stage == nova::ShaderStage::Mesh) {
                    stage_flags[count] = VK_SHADER_STAGE_VERTEX_BIT;
                    shader_objects[count++] = VK_NULL_HANDLE;

                } else if (shaders[i]->stage == nova::ShaderStage::Vertex) {
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