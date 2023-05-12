#include "Renderer.hpp"

namespace pyr
{
    static mat4 ProjInfReversedZRH(f32 fovY, f32 aspectWbyH, f32 zNear)
    {
        // https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/

        f32 f = 1.f / glm::tan(fovY / 2.f);
        mat4 proj{};
        proj[0][0] = f / aspectWbyH;
        proj[1][1] = f;
        proj[3][2] = zNear; // Right, middle-bottom
        proj[2][3] = -1.f;  // Bottom, middle-right
        return proj;
    }

    void Renderer::Init(Context& _ctx)
    {
        ctx = &_ctx;

// -----------------------------------------------------------------------------

        VkCall(vkCreateDescriptorSetLayout(ctx->device, Temp(VkDescriptorSetLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = Temp(VkDescriptorSetLayoutBindingFlagsCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = 1,
                .pBindingFlags = Temp<VkDescriptorBindingFlags>(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT),
            }),
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
            .bindingCount = 1,
            .pBindings = Temp(VkDescriptorSetLayoutBinding {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 65'536,
                .stageFlags = VK_SHADER_STAGE_ALL,
            }),
        }), nullptr, &textureDescriptorSetLayout));

        VkDeviceSize descriptorSize;
        vkGetDescriptorSetLayoutSizeEXT(ctx->device, textureDescriptorSetLayout, &descriptorSize);
        textureDescriptorBuffer = ctx->CreateBuffer(descriptorSize,
            VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT, BufferFlags::DeviceLocal | BufferFlags::CreateMapped);

// -----------------------------------------------------------------------------
//                       Rasterization vertex shader
// -----------------------------------------------------------------------------

        vertexShader = ctx->CreateShader(
            VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT,
            "assets/shaders/vertex-generated",
            R"(
#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64  : require
#extension GL_EXT_buffer_reference2 : require

layout(buffer_reference, scalar) buffer PositionBR { vec3 value[]; };

layout(push_constant) uniform PushConstants
{
    mat4 mvp;
    uint64_t vertices;
    uint64_t material;
    uint64_t vertexOffset;
    uint vertexStride;
} pc;

layout(location = 0) out uint outVertexIndex;

void main()
{
    vec3 pos = PositionBR(pc.vertices + pc.vertexOffset + (gl_VertexIndex * pc.vertexStride)).value[0];
    outVertexIndex = gl_VertexIndex;
    gl_Position = pc.mvp * vec4(pos, 1);
}
            )",
            {{
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .size = sizeof(RasterPushConstants),
            }},
            {
                textureDescriptorSetLayout
            });

        VkCall(vkCreatePipelineLayout(ctx->device, Temp(VkPipelineLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &textureDescriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = Temp(VkPushConstantRange {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .size = sizeof(RasterPushConstants),
            }),
        }), nullptr, &layout));

// -----------------------------------------------------------------------------
//                           Compositing shaders
// -----------------------------------------------------------------------------

        VkCall(vkCreateDescriptorSetLayout(ctx->device, Temp(VkDescriptorSetLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
            .bindingCount = 2,
            .pBindings = std::array {
                VkDescriptorSetLayoutBinding {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                },
                VkDescriptorSetLayoutBinding {
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                },
            }.data(),
        }), nullptr, &compositeDescriptorLayout));

        compositeShader = ctx->CreateShader(
            VK_SHADER_STAGE_COMPUTE_BIT, 0,
            "assets/shaders/compute-generated",
            R"(
#version 460

layout(set = 0, binding = 0, rgba16f) uniform image2D inAccum;
layout(set = 0, binding = 1, rgba8) uniform image2D outColor;

layout(local_size_x = 1) in;
void main()
{
    uvec2 pos = gl_GlobalInvocationID.xy;
    ivec2 coords = ivec2(pos);

    vec4 color = imageLoad(inAccum, coords);
    imageStore(outColor, coords, color);
}
            )",
            {},
            {
                compositeDescriptorLayout,
            });

        VkCall(vkCreatePipelineLayout(ctx->device, Temp(VkPipelineLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &compositeDescriptorLayout,
        }), nullptr, &compositePipelineLayout));

// -----------------------------------------------------------------------------

        materialBuffer = ctx->CreateBuffer(MaxMaterials * MaterialSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, BufferFlags::DeviceLocal | BufferFlags::CreateMapped);

// -----------------------------------------------------------------------------
//                   Prepare Top-Level acceleration structure
// -----------------------------------------------------------------------------

    {
        auto geom = VkAccelerationStructureGeometryKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = {
                .instances =  {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                },
            },
        };

        auto buildInfo = VkAccelerationStructureBuildGeometryInfoKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
                | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR
                ,
            .geometryCount = 1,
            .pGeometries = &geom,
        };

        auto sizes = VkAccelerationStructureBuildSizesInfoKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        };

        vkGetAccelerationStructureBuildSizesKHR(ctx->device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            &MaxInstances,
            &sizes);

        sizes.buildScratchSize += AccelScratchAlignment;

        PYR_LOG("TLAS Statistics - scratch = {}, storage = {}", sizes.buildScratchSize, sizes.accelerationStructureSize);

        tlasBuffer = ctx->CreateBuffer(sizes.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, BufferFlags::DeviceLocal);

        tlasScratchBuffer = ctx->CreateBuffer(sizes.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, BufferFlags::DeviceLocal);

        VkCall(vkCreateAccelerationStructureKHR(ctx->device, Temp(VkAccelerationStructureCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = tlasBuffer.buffer,
            .size = tlasBuffer.size,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        }), nullptr, &tlas));

        PYR_LOG("Created TLAS: {}", (void*)tlas);

        instanceBuffer = ctx->CreateBuffer(MaxInstances * sizeof(VkAccelerationStructureInstanceKHR),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            BufferFlags::DeviceLocal | BufferFlags::CreateMapped);
    }

// -----------------------------------------------------------------------------
//                       Prepare Shader binding table
// -----------------------------------------------------------------------------

        {
            VkCall(vkCreateDescriptorSetLayout(ctx->device, Temp(VkDescriptorSetLayoutCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
                .bindingCount = 3,
                .pBindings = std::array {
                    VkDescriptorSetLayoutBinding {
                        .binding = 0,
                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        .descriptorCount = 1,
                        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                    },
                    VkDescriptorSetLayoutBinding {
                        .binding = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                        .descriptorCount = 1,
                        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                    },
                    VkDescriptorSetLayoutBinding {
                        .binding = 2,
                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        .descriptorCount = 1,
                        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                    },
                }.data(),
            }), nullptr, &rtDescLayout));

//             rayHitShader = ctx->CreateShader(
//                 VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0,
//                 "rayhit",
//                 R"(
// #version 460

// #extension GL_EXT_scalar_block_layout : require
// #extension GL_EXT_shader_explicit_arithmetic_types_int64  : require
// #extension GL_EXT_buffer_reference2 : require
// #extension GL_EXT_ray_tracing : require
// #extension GL_EXT_ray_tracing_position_fetch : require

// struct RayPayload
// {
//     vec3 color;
// };
// layout(location = 0) rayPayloadInEXT RayPayload rayPayload;

// hitAttributeEXT vec3 barycentric;

// void main()
// {
//     // rayPayload.position[0] = gl_HitTriangleVertexPositionsEXT[0];
//     // rayPayload.position[1] = gl_HitTriangleVertexPositionsEXT[1];
//     // rayPayload.position[2] = gl_HitTriangleVertexPositionsEXT[2];

//     vec3 w = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);

//     vec3 v0 = gl_HitTriangleVertexPositionsEXT[0];
//     vec3 v1 = gl_HitTriangleVertexPositionsEXT[1];
//     vec3 v2 = gl_HitTriangleVertexPositionsEXT[2];

//     vec3 v01 = v1 - v0;
//     vec3 v02 = v2 - v0;
//     vec3 nrm = normalize(cross(v01, v02));
//     if (dot(gl_WorldRayDirectionEXT, nrm) > 0)
//         nrm = -nrm;

//     rayPayload.color = nrm * 0.5 + 0.5;
// }
//                 )",
//                 {},
//                 {});

            rayMissShader = ctx->CreateShader(
                VK_SHADER_STAGE_MISS_BIT_KHR, 0,
                "raymiss",
                R"(
#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64  : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_ray_tracing : require

struct RayPayload
{
    vec3 color;
};
layout(location = 0) rayPayloadInEXT RayPayload rayPayload;

void main()
{
    // rayPayload.position[0] = vec3(1, 0, 0);
    // rayPayload.position[1] = vec3(1, 0, 0);
    // rayPayload.position[2] = vec3(1, 0, 0);
    rayPayload.color = vec3(0.5, 0.5, 0);
}
                )",
                {},
                {});

            rayGenShader = ctx->CreateShader(
                VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0,
                "raygen",
                R"(
#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64  : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_ray_tracing : require
#extension GL_NV_shader_invocation_reorder : require

layout(set = 0, binding = 0, rgba8) uniform image2D outImage;
layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 2, r32ui) uniform uimage2D objectIdImage;

struct RayPayload
{
    vec3 color;
};
layout(location = 0) rayPayloadEXT RayPayload rayPayload;

layout(location = 0) hitObjectAttributeNV vec3 barycentric;

layout(push_constant) uniform PushConstants
{
    vec3 pos;
    vec3 camX;
    vec3 camY;
    uint64_t objectsVA;
    uint64_t meshesVA;
    float camZOffset;
    uint debugMode;
} pc;

void main()
{
    imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(0, 0, 0, 1));

    const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy);
    const vec2 inUV = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
    vec2 d = inUV * 2.0 - 1.0;
    vec3 focalPoint = pc.camZOffset * cross(pc.camX, pc.camY);
    vec3 pos = pc.pos;
    d.x *= float(gl_LaunchSizeEXT.x) / float(gl_LaunchSizeEXT.y);
    d.y *= -1.0;
    vec3 dir = normalize((pc.camY * d.y) + (pc.camX * d.x) - focalPoint);

    uint rayFlags = 0;
    rayFlags |= gl_RayFlagsOpaqueEXT;

    hitObjectNV hitObject;
    hitObjectTraceRayNV(hitObject, topLevelAS, rayFlags, 0xFF, 0, 0, 1, pos, 0.0001, dir, 8000000, 0);

    reorderThreadNV(hitObject);

    if (hitObjectIsHitNV(hitObject))
    {
        hitObjectGetAttributesNV(hitObject, 0);
        hitObjectExecuteShaderNV(hitObject, 0);

        // vec3 w = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);

        // vec3 v0 = rayPayload.position[0];
        // vec3 v1 = rayPayload.position[1];
        // vec3 v2 = rayPayload.position[2];

        // vec3 v01 = v1 - v0;
        // vec3 v02 = v2 - v0;
        // vec3 nrm = normalize(cross(v01, v02));
        // if (dot(dir, nrm) > 0)
        //     nrm = -nrm;

        // vec3 color = nrm * 0.5 + 0.5;

        imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(rayPayload.color, 1));
    }
    else
    {
        imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(0.1, 0.1, 0.1, 1));
    }
}
                )",
                {{
                    .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                    .size = sizeof(RayTracePC),
                }},
                {
                    rtDescLayout
                });

            VkCall(vkCreatePipelineLayout(ctx->device, Temp(VkPipelineLayoutCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &rtDescLayout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = Temp(VkPushConstantRange {
                    .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                    .size = sizeof(RayTracePC),
                }),
            }), nullptr, &rtPipelineLayout));
        }
    }

    void Renderer::RebuildShaderBindingTable()
    {
        if (rtPipeline)
            vkDestroyPipeline(ctx->device, rtPipeline, nullptr);
        ctx->DestroyBuffer(sbtBuffer);

        struct HitShaderGroup
        {
            Shader* closestHitShader;
            Shader* anyHitShader;
            Shader* intersectionShader;
        };

        std::vector<Shader*> rayGenShaders;
        std::vector<Shader*> rayMissShaders;
        std::vector<HitShaderGroup> rayHitShaderGroups;
        std::vector<Shader*> callableShaders;

        // Add shaders

        rayGenShaders.push_back(&rayGenShader);
        rayMissShaders.push_back(&rayMissShader);
        materialTypes.ForEach([&](auto, auto& materialType) {
            materialType.sbtOffset = rayHitShaderGroups.size();
            rayHitShaderGroups.push_back(HitShaderGroup {
                .closestHitShader = &materialType.closestHitShader,
            });
        });

        // Convert to stages and groups

        u32 nextStageIndex = 0;
        ankerl::unordered_dense::map<VkShaderModule, u32> stageIndices;
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::vector<u32> rayGenIndices, rayMissIndices, rayHitIndices, rayCallIndices;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

        auto getShaderIndex = [&](Shader* shader) {
            if (!shader)
                return VK_SHADER_UNUSED_KHR;

            if (!stageIndices.contains(shader->module))
            {
                u32 index = nextStageIndex++;
                stageIndices.insert({ shader->module, index });
                stages.push_back(shader->stageInfo);
                return index;
            }

            return stageIndices.at(shader->module);
        };

        auto createGroup = [&]() -> auto& {
            auto& info = groups.emplace_back();
            info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            info.generalShader = info.closestHitShader = info.anyHitShader = info.intersectionShader = VK_SHADER_UNUSED_KHR;
            return info;
        };

        for (auto& shader : rayGenShaders)
        {
            rayGenIndices.push_back(groups.size());
            createGroup().generalShader = getShaderIndex(shader);
        }

        for (auto& shader : rayMissShaders)
        {
            rayMissIndices.push_back(groups.size());
            createGroup().generalShader = getShaderIndex(shader);
        }

        for (auto& group : rayHitShaderGroups)
        {
            rayHitIndices.push_back(groups.size());
            auto& info = createGroup();
            info.type = group.intersectionShader
                ? VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR
                : VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            info.closestHitShader = getShaderIndex(group.closestHitShader);
            info.anyHitShader = getShaderIndex(group.anyHitShader);
            info.intersectionShader = getShaderIndex(group.intersectionShader);
        }

        for (auto& shader : callableShaders)
        {
            rayCallIndices.push_back(groups.size());
            createGroup().generalShader = getShaderIndex(shader);
        }

        // Debug

        PYR_LOG("---- stages ----");
        for (auto& stage : stages)
            PYR_LOG(" - stage.module = {}", (void*)stage.module);
        PYR_LOG("---- groups ----");
        for (auto& group : groups)
            PYR_LOG(" - group.type = {}, general = {}, closest hit = {}, any hit = {}",
                (i32)group.type, group.generalShader, group.closestHitShader, group.anyHitShader);
        PYR_LOG("----");

        // Create pipeline

        VkCall(vkCreateRayTracingPipelinesKHR(ctx->device, 0, nullptr, 1, Temp(VkRayTracingPipelineCreateInfoKHR {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
            .stageCount = u32(stages.size()),
            .pStages = stages.data(),
            .groupCount = u32(groups.size()),
            .pGroups = groups.data(),
            .maxPipelineRayRecursionDepth = 2,
            .layout = rtPipelineLayout,
        }), nullptr, &rtPipeline));

        // Compute table parameters

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayProperties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
        vkGetPhysicalDeviceProperties2(ctx->gpu, Temp(VkPhysicalDeviceProperties2 {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &rayProperties,
        }));

        u32 handleSize = rayProperties.shaderGroupHandleSize;
        u32 handleStride = u32(AlignUpPower2(handleSize, rayProperties.shaderGroupHandleAlignment));
        u64 groupAlign = rayProperties.shaderGroupBaseAlignment;
        u64 rayMissOffset = AlignUpPower2(rayGenIndices.size() * handleStride, groupAlign);
        u64 rayHitOffset = rayMissOffset + AlignUpPower2(rayMissIndices.size() * handleStride, groupAlign);
        u64 rayCallOffset = rayHitOffset + AlignUpPower2(rayHitIndices.size() * handleStride, groupAlign);
        u64 tableSize = rayCallOffset + rayCallIndices.size() * handleStride;

        // Allocate table and get groups from pipeline

        sbtBuffer = ctx->CreateBuffer(std::max(256ull, tableSize),
            VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
            BufferFlags::DeviceLocal | BufferFlags::CreateMapped);
        auto getMapped = [&](u32 offset, u32 i) { return sbtBuffer.mapped + offset + (i * handleStride); };

        std::vector<u8> handles(groups.size() * handleSize);
        VkCall(vkGetRayTracingShaderGroupHandlesKHR(ctx->device, rtPipeline, 0, u32(groups.size()), u32(handles.size()), handles.data()));
        auto getHandle = [&](u32 i) { return handles.data() + i * handleSize; };

        // Gen

        rayGenRegion.size = handleSize;
        rayGenRegion.stride = handleSize;
        for (u32 i = 0; i < rayGenIndices.size(); ++i)
            std::memcpy(getMapped(0, i), getHandle(rayGenIndices[i]), handleSize);

        // Miss

        rayMissRegion.deviceAddress = sbtBuffer.address + rayMissOffset;
        rayMissRegion.size = rayHitOffset - rayMissOffset;
        rayMissRegion.stride = handleStride;
        for (u32 i = 0; i < rayMissIndices.size(); ++i)
            std::memcpy(getMapped(rayMissOffset, i), getHandle(rayMissIndices[i]), handleSize);

        // Hit

        rayHitRegion.deviceAddress = sbtBuffer.address + rayHitOffset;
        rayHitRegion.size = rayCallOffset - rayHitOffset;
        rayHitRegion.stride = handleStride;
        for (u32 i = 0; i < rayHitIndices.size(); ++i)
            std::memcpy(getMapped(rayHitOffset, i), getHandle(rayHitIndices[i]), handleSize);

        // Call

        rayCallRegion.deviceAddress = sbtBuffer.address + rayCallOffset;
        rayCallRegion.size = tableSize - rayCallOffset;
        rayCallRegion.stride = handleStride;
        for (u32 i = 0; i < rayCallIndices.size(); ++i)
            std::memcpy(getMapped(rayCallOffset, i), getHandle(rayCallIndices[i]), handleSize);
    }

    void Renderer::SetCamera(vec3 position, quat rotation, f32 fov)
    {
        viewPosition = position;
        viewRotation = rotation;
        viewFov = fov;
    }

    void Renderer::Draw(Image& target)
    {
        auto cmd = ctx->cmd;

        // Resize depth buffer

        if (target.extent != lastExtent)
        {
            lastExtent = target.extent;

            ctx->DestroyImage(accumImage);
            accumImage = ctx->CreateImage(target.extent,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                | VK_IMAGE_USAGE_STORAGE_BIT,
                VK_FORMAT_R16G16B16A16_SFLOAT);

            ctx->DestroyImage(depthBuffer);
            depthBuffer = ctx->CreateImage(target.extent,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_FORMAT_X8_D24_UNORM_PACK32);
            ctx->Transition(cmd, depthBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        }

        // Begin rendering

        if (!rayTrace)
        {
            ctx->Transition(cmd, accumImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            ctx->Transition(cmd, target, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            vkCmdBeginRendering(cmd, Temp(VkRenderingInfo {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = { {}, { target.extent.x, target.extent.y } },
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = std::array {
                    VkRenderingAttachmentInfo {
                        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        .imageView = accumImage.view,
                        .imageLayout = accumImage.layout,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                        .clearValue = {{{ 0.1f, 0.1f, 0.1f, 1.f }}},
                    },
                }.data(),
                .pDepthAttachment = Temp(VkRenderingAttachmentInfo {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = depthBuffer.view,
                    .imageLayout = depthBuffer.layout,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .clearValue = { .depthStencil = {{ 0.f }} },
                })
            }));

            vkCmdSetScissorWithCount(cmd, 1, Temp(VkRect2D { {}, { target.extent.x, target.extent.y } }));
            vkCmdSetViewportWithCount(cmd, 1, Temp(VkViewport {
                0.f, f32(target.extent.y),
                f32(target.extent.x), -f32(target.extent.y),
                0.f, 1.f
            }));

            // Set blend, depth, cull dynamic state

            b8 blend = false;
            b8 depth = true;
            b8 cull = false;
            b8 wireframe = false;

            if (depth)
            {
                vkCmdSetDepthTestEnable(cmd, true);
                vkCmdSetDepthWriteEnable(cmd, true);
                vkCmdSetDepthCompareOp(cmd, VK_COMPARE_OP_GREATER);
            }

            if (cull)
            {
                vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);
                vkCmdSetFrontFace(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            }

            vkCmdSetColorWriteMaskEXT(cmd, 0, 1, Temp<VkColorComponentFlags>(
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            ));
            if (blend)
            {
                vkCmdSetColorBlendEnableEXT(cmd, 0, 1, Temp<VkBool32>(true));
                vkCmdSetColorBlendEquationEXT(cmd, 0, 1, Temp(VkColorBlendEquationEXT {
                    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                }));
            }
            else
            {
                vkCmdSetColorBlendEnableEXT(cmd, 0, 1, Temp<VkBool32>(false));
            }

            if (wireframe)
            {
                vkCmdSetPolygonModeEXT(cmd, VK_POLYGON_MODE_LINE);
                vkCmdSetLineWidth(cmd, 1.f);
            }

            // Bind shaders and draw

            auto proj = ProjInfReversedZRH(viewFov, f32(target.extent.x) / f32(target.extent.y), 0.01f);
            auto translate = glm::translate(mat4(1.f), viewPosition);
            auto rotate = glm::mat4_cast(viewRotation);
            auto viewProj = proj * glm::affineInverse(translate * rotate);

            vkCmdSetPrimitiveTopology(cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

            vkCmdBindDescriptorBuffersEXT(cmd, 1, Temp(VkDescriptorBufferBindingInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                .address = textureDescriptorBuffer.address,
                .usage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
            }));
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, Temp(0u), Temp(0ull));

            objects.ForEach([&](auto, auto& object) {
                auto& mesh = meshes.Get(object.meshID);

                auto transform = glm::translate(glm::mat4(1.f), object.position);
                transform *= glm::mat4_cast(object.rotation);
                transform *= glm::scale(glm::mat4(1.f), object.scale);

                auto& material = materials.Get(object.materialID);
                auto& materialType = materialTypes.Get(material.materialTypeID);

                vkCmdBindShadersEXT(cmd, 2,
                    std::array { vertexShader.stage, materialType.fragmentShader.stage }.data(),
                    std::array { vertexShader.shader, materialType.fragmentShader.shader }.data());
                vkCmdBindIndexBuffer(cmd, mesh.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdPushConstants(cmd, layout,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0, sizeof(RasterPushConstants), Temp(RasterPushConstants {
                        .mvp = viewProj * transform,
                        .vertices = mesh.vertices.address,
                        .material = material.data,
                        .vertexOffset = mesh.vertexOffset,
                        .vertexStride = mesh.vertexStride,
                    }));

                vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
            });

            // End rendering

            vkCmdEndRendering(cmd);

            ctx->Transition(cmd, accumImage, VK_IMAGE_LAYOUT_GENERAL);
            ctx->Transition(cmd, target, VK_IMAGE_LAYOUT_GENERAL);
            vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compositePipelineLayout, 0, 2, std::array {
                VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .pImageInfo = Temp(VkDescriptorImageInfo {
                        .imageView = accumImage.view,
                        .imageLayout = accumImage.layout,
                    })
                },
                VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstBinding = 1,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .pImageInfo = Temp(VkDescriptorImageInfo {
                        .imageView = target.view,
                        .imageLayout = target.layout,
                    })
                },
            }.data());
            vkCmdBindShadersEXT(cmd, 1, &compositeShader.stage, &compositeShader.shader);
            vkCmdDispatch(cmd, target.extent.x, target.extent.y, 1);
        }
        else
        {
            auto geom = VkAccelerationStructureGeometryKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
                .geometry = {
                    .instances =  {
                        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                        .data = {{ instanceBuffer.address }},
                    },
                },
            };

            auto buildRange = VkAccelerationStructureBuildRangeInfoKHR {
                .primitiveCount = objects.GetCount(),
            };

            u32 i = 0;
            objects.ForEach([&](auto, Object& object) {
                auto& mesh = meshes.Get(object.meshID);

                auto& material = materials.Get(object.materialID);
                auto& materialType = materialTypes.Get(material.materialTypeID);

                auto transform = glm::translate(glm::mat4(1.f), object.position);
                transform *= glm::mat4_cast(object.rotation);
                transform *= glm::scale(glm::mat4(1.f), object.scale);

                auto& M = transform;

                instanceBuffer.Get<VkAccelerationStructureInstanceKHR>(i) = VkAccelerationStructureInstanceKHR {
                    .transform = {
                        M[0][0], M[1][0], M[2][0], M[3][0],
                        M[0][1], M[1][1], M[2][1], M[3][1],
                        M[0][2], M[1][2], M[2][2], M[3][2],
                    },
                    .instanceCustomIndex = i,
                    .mask = 0xFF,
                    .instanceShaderBindingTableRecordOffset = materialType.sbtOffset,
                    .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
                    .accelerationStructureReference = mesh.accelAddress,
                };

                i++;
            });

            auto buildInfo = VkAccelerationStructureBuildGeometryInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
                .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
                    | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR
                    ,
                .dstAccelerationStructure = tlas,
                .geometryCount = 1,
                .pGeometries = &geom,
                .scratchData = {{ AlignUpPower2(tlasScratchBuffer.address, AccelScratchAlignment) }},
            };

            vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, Temp(&buildRange));

            ctx->Flush();

            ctx->Transition(cmd, target, VK_IMAGE_LAYOUT_GENERAL);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);

            vkCmdPushDescriptorSetKHR(cmd,
                VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                rtPipelineLayout,
                0, 2, std::array {
                    VkWriteDescriptorSet {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstBinding = 0,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        .pImageInfo = Temp(VkDescriptorImageInfo {
                            .imageView = target.view,
                            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                        })
                    },
                    VkWriteDescriptorSet {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .pNext = Temp(VkWriteDescriptorSetAccelerationStructureKHR {
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                            .accelerationStructureCount = 1,
                            .pAccelerationStructures = &tlas,
                        }),
                        .dstBinding = 1,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                    },
                }.data());

            auto camX = viewRotation * glm::vec3(1, 0, 0);
            auto camY = viewRotation * glm::vec3(0, 1, 0);
            auto camZOffset = 1.f / glm::tan(0.5f * viewFov);

            vkCmdPushConstants(cmd, rtPipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                0, sizeof(RayTracePC), Temp(RayTracePC {
                    .pos = viewPosition,
                    .camX = camX,
                    .camY = camY,
                    .objectsVA = 0,
                    .meshesVA = 0,
                    .camZOffset = camZOffset,
                    .debugMode = 0,
                }));

            u32 activeRayGenShader = 0;
            rayGenRegion.deviceAddress = sbtBuffer.address + (rayGenRegion.stride * activeRayGenShader);
            vkCmdTraceRaysKHR(cmd,
                &rayGenRegion, &rayMissRegion, &rayHitRegion, &rayCallRegion,
                target.extent.x, target.extent.y, 1);
        }
    }

    MeshID Renderer::CreateMesh(
        usz dataSize, const void* pData,
        u32 vertexStride, usz vertexOffset,
        u32 indexCount, const u32* pIndices)
    {
        auto[id, mesh] = meshes.Acquire();

        mesh.vertices = ctx->CreateBuffer(dataSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, BufferFlags::DeviceLocal);
        ctx->CopyToBuffer(mesh.vertices, pData, dataSize);

        mesh.vertexOffset = vertexOffset;
        mesh.vertexStride = vertexStride;

        auto indexSize = sizeof(u32) * indexCount;
        mesh.indices = ctx->CreateBuffer(indexSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, BufferFlags::DeviceLocal);
        ctx->CopyToBuffer(mesh.indices, pIndices, indexSize);

        mesh.indexCount = indexCount;

        {
            // BLAS

            u32 maxIndex = 0;
            for (auto i = 0; i < indexCount; ++i)
                maxIndex = std::max(maxIndex, pIndices[indexCount]);

            auto geometry = VkAccelerationStructureGeometryKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                .geometry = {
                    .triangles = {
                        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                        .vertexData = {{ mesh.vertices.address + vertexOffset }},
                        .vertexStride = vertexStride,
                        .maxVertex = maxIndex,
                        .indexType = VK_INDEX_TYPE_UINT32,
                        .indexData = {{ mesh.indices.address }},
                    },
                },
            };

            auto buildRange = VkAccelerationStructureBuildRangeInfoKHR {
                .primitiveCount = indexCount / 3,
            };

            auto buildInfo = VkAccelerationStructureBuildGeometryInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
                    | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR
                    ,
                .geometryCount = 1,
                .pGeometries = &geometry,
            };

            // Compute sizes

            auto sizes = VkAccelerationStructureBuildSizesInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
            };
            vkGetAccelerationStructureBuildSizesKHR(ctx->device,
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &buildInfo, &buildRange.primitiveCount, &sizes);

            constexpr VkDeviceSize accelScratchAlignment = 128;
            sizes.buildScratchSize += accelScratchAlignment;

            PYR_LOG("Building BLAS for mesh, triangles = {}", buildRange.primitiveCount);
            PYR_LOG("  scratch size = {}, storage size = {}", sizes.buildScratchSize, sizes.accelerationStructureSize);

            auto scratchBuffer = ctx->CreateBuffer(sizes.buildScratchSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, BufferFlags::DeviceLocal);
            mesh.accelBuffer = ctx->CreateBuffer(sizes.accelerationStructureSize,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, BufferFlags::DeviceLocal);

            // Build

            VkCall(vkCreateAccelerationStructureKHR(ctx->device, Temp(VkAccelerationStructureCreateInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                .buffer = mesh.accelBuffer.buffer,
                .size = mesh.accelBuffer.size,
                .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            }), nullptr, &mesh.accelStructure));

            mesh.accelAddress = vkGetAccelerationStructureDeviceAddressKHR(ctx->device, Temp(VkAccelerationStructureDeviceAddressInfoKHR {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .accelerationStructure = mesh.accelStructure,
            }));

            buildInfo.dstAccelerationStructure = mesh.accelStructure;
            buildInfo.scratchData.deviceAddress = AlignUpPower2(scratchBuffer.address, accelScratchAlignment);

            vkCmdBuildAccelerationStructuresKHR(ctx->transferCmd, 1, &buildInfo, Temp(&buildRange));
            ctx->Flush(ctx->transferCmd);

            ctx->DestroyBuffer(scratchBuffer);

            PYR_LOG("Built BLAS!");
        }

        return id;
    }

    void Renderer::DeleteMesh(MeshID id)
    {
        auto& mesh = meshes.Get(id);
        ctx->DestroyBuffer(mesh.vertices);
        ctx->DestroyBuffer(mesh.indices);

        vkDestroyAccelerationStructureKHR(ctx->device, mesh.accelStructure, nullptr);
        ctx->DestroyBuffer(mesh.accelBuffer);

        meshes.Return(id);
    }

    MaterialTypeID Renderer::CreateMaterialType(const char* pShader, b8 inlineData)
    {
        auto[id, materialType] = materialTypes.Acquire();
        materialType.fragmentShader = ctx->CreateShader(
            VK_SHADER_STAGE_FRAGMENT_BIT, 0,
            "assets/shaders/fragment-generated",
            std::format("{}{}{}",
            R"(
#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64  : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_fragment_shader_barycentric : require
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) in pervertexEXT uint vertexIndex[3];

layout(location = 0) out vec4 outAccum;

layout(push_constant) uniform PushConstants
{
    mat4 mvp;
    uint64_t vertices;
    uint64_t material;
    uint64_t vertexOffset;
    uint vertexStride;
} pc;
            )",
            pShader,
            R"(
void main()
{
    outAccum = shade(
        pc.vertices, pc.material,
        uvec3(vertexIndex[0], vertexIndex[1], vertexIndex[2]),
        gl_BaryCoordEXT);

    if (outAccum.a == 0.0)
        discard;
}
            )"),
            {{
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .size = sizeof(RasterPushConstants),
            }},
            {
                textureDescriptorSetLayout
            });
        materialType.inlineData = inlineData;

        materialType.closestHitShader = ctx->CreateShader(
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0,
            "rayhit",
            R"(
#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64  : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_ray_tracing_position_fetch : require

struct RayPayload
{
    vec3 color;
};
layout(location = 0) rayPayloadInEXT RayPayload rayPayload;

hitAttributeEXT vec3 barycentric;

void main()
{
    // rayPayload.position[0] = gl_HitTriangleVertexPositionsEXT[0];
    // rayPayload.position[1] = gl_HitTriangleVertexPositionsEXT[1];
    // rayPayload.position[2] = gl_HitTriangleVertexPositionsEXT[2];

    vec3 w = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);

    vec3 v0 = gl_HitTriangleVertexPositionsEXT[0];
    vec3 v1 = gl_HitTriangleVertexPositionsEXT[1];
    vec3 v2 = gl_HitTriangleVertexPositionsEXT[2];

    vec3 v01 = v1 - v0;
    vec3 v02 = v2 - v0;
    vec3 nrm = normalize(cross(v01, v02));
    if (dot(gl_WorldRayDirectionEXT, nrm) > 0)
        nrm = -nrm;

    rayPayload.color = nrm * 0.5 + 0.5;
}
            )",
            {},
            {});

        return id;
    }

    void Renderer::DeleteMaterialType(MaterialTypeID id)
    {
        auto& material = materialTypes.Get(id);
        ctx->DestroyShader(material.fragmentShader);
        materialTypes.Return(id);
    }

    MaterialID Renderer::CreateMaterial(MaterialTypeID matTypeID, const void* pData, usz size)
    {
        auto[id, material] = materials.Acquire();
        material.materialTypeID = matTypeID;

        if (pData && size > 0)
        {
            if (materialTypes.Get(matTypeID).inlineData)
            {
                if (size > 8)
                    PYR_THROW("Inline material data must not exceed 8 bytes");

                std::memcpy(&material.data, pData, size);
            }
            else if (size < 64)
            {
                // TODO: Better material allocation strategy
                //  - don't consume buffer space for inline materials
                //  - allow packaged varaible size materials (16, 32, 64)

                u64 offset = MaterialSize * u32(id);
                std::memcpy(materialBuffer.mapped + offset, pData, size);
                material.data = materialBuffer.address + offset;
            }
            else
            {
                // Large materials use dedicated buffer

                material.buffer = ctx->CreateBuffer(size,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    BufferFlags::DeviceLocal | BufferFlags::CreateMapped);
                ctx->CopyToBuffer(material.buffer, pData, size);
                material.data = material.buffer.address;
            }
        }

        return id;
    }

    void Renderer::DeleteMaterial(MaterialID id)
    {
        ctx->DestroyBuffer(materials.Get(id).buffer);
        materials.Return(id);
    }

    ObjectID Renderer::CreateObject(MeshID meshID, MaterialID materialID, vec3 position, quat rotation, vec3 scale)
    {
        auto[id, object] = objects.Acquire();
        object.meshID = meshID;
        object.materialID = materialID;
        object.position = position;
        object.rotation = rotation;
        object.scale = scale;

        return id;
    }

    void Renderer::DeleteObject(ObjectID id)
    {
        objects.Return(id);
    }

    TextureID Renderer::RegisterTexture(VkImageView view, VkSampler sampler)
    {
        // TODO: Support registering multiple textures contiguously

        auto[id, texture] = textures.Acquire();
        texture.index = u32(id);

        vkGetDescriptorEXT(ctx->device,
            Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .data = {
                    .pCombinedImageSampler = Temp(VkDescriptorImageInfo {
                        .sampler = sampler,
                        .imageView = view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // TODO: configurable?
                    }),
                },
            }),
            ctx->descriptorSizes.combinedImageSamplerDescriptorSize,
            textureDescriptorBuffer.mapped + (u32(id) * ctx->descriptorSizes.combinedImageSamplerDescriptorSize));

        return id;
    }

    void Renderer::UnregisterTexture(TextureID id)
    {
        textures.Return(id);
    }
}