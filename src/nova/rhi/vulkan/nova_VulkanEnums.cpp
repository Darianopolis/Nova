#include "nova_VulkanRHI.hpp"

namespace nova
{
    VkBufferUsageFlags GetVulkanBufferUsage(BufferUsage usage)
    {
        VkBufferUsageFlags out = 0;

        if (usage >= BufferUsage::TransferSrc)         { out |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT; }
        if (usage >= BufferUsage::TransferDst)         { out |= VK_BUFFER_USAGE_TRANSFER_DST_BIT; }
        if (usage >= BufferUsage::Uniform)             { out |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; }
        if (usage >= BufferUsage::Storage)             { out |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; }
        if (usage >= BufferUsage::Index)               { out |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT; }
        if (usage >= BufferUsage::Vertex)              { out |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; }
        if (usage >= BufferUsage::Indirect)            { out |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT; }
        if (usage >= BufferUsage::ShaderBindingTable)  { out |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR; }
        if (usage >= BufferUsage::AccelBuild)          { out |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR; }
        if (usage >= BufferUsage::AccelStorage)        { out |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR; }
        if (usage >= BufferUsage::DescriptorSamplers)  { out |= VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT; }
        if (usage >= BufferUsage::DescriptorResources) { out |= VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT; }

        return out;
    }

    VkImageUsageFlags GetVulkanImageUsage(TextureUsage usage)
    {
        VkImageUsageFlags out = 0;

        if (usage >= TextureUsage::TransferSrc)        { out |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; }
        if (usage >= TextureUsage::TransferDst)        { out |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; }
        if (usage >= TextureUsage::Sampled)            { out |= VK_IMAGE_USAGE_SAMPLED_BIT; }
        if (usage >= TextureUsage::Storage)            { out |= VK_IMAGE_USAGE_STORAGE_BIT; }
        if (usage >= TextureUsage::ColorAttach)        { out |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; }
        if (usage >= TextureUsage::DepthStencilAttach) { out |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; }

        return out;
    }

    VkFormat GetVulkanFormat(Format format)
    {
        switch (format) {
        break;case Format::Undefined:     return VK_FORMAT_UNDEFINED;
        break;case Format::RGBA8_UNorm:   return VK_FORMAT_R8G8B8A8_UNORM;
        break;case Format::RGBA8_SRGB:    return VK_FORMAT_R8G8B8A8_SRGB;
        break;case Format::RGBA16_SFloat: return VK_FORMAT_R16G16B16A16_SFLOAT;
        break;case Format::RGBA32_SFloat: return VK_FORMAT_R32G32B32A32_SFLOAT;
        break;case Format::BGRA8_UNorm:   return VK_FORMAT_B8G8R8A8_UNORM;
        break;case Format::BGRA8_SRGB:    return VK_FORMAT_B8G8R8A8_SRGB;
        break;case Format::RGB32_SFloat:  return VK_FORMAT_R32G32B32_SFLOAT;
        break;case Format::R8_UNorm:      return VK_FORMAT_R8_UNORM;
        break;case Format::RG8_UNorm:     return VK_FORMAT_R8G8_UNORM;
        break;case Format::R32_SFloat:    return VK_FORMAT_R32_SFLOAT;
        break;case Format::R8_UInt:       return VK_FORMAT_R8_UINT;
        break;case Format::R16_UInt:      return VK_FORMAT_R16_UINT;
        break;case Format::R32_UInt:      return VK_FORMAT_R32_UINT;
        break;case Format::D24_UNorm:     return VK_FORMAT_X8_D24_UNORM_PACK32;
        break;case Format::S8_D24_UNorm:  return VK_FORMAT_D24_UNORM_S8_UINT;
        break;case Format::D32_SFloat:    return VK_FORMAT_D32_SFLOAT;
        break;case Format::BC7_SRGB:      return VK_FORMAT_BC7_SRGB_BLOCK;
        break;case Format::BC7_Unorm:     return VK_FORMAT_BC7_UNORM_BLOCK;
        }

        NOVA_THROW("Unknown Format: {}", u32(format));
    }

    Format FromVulkanFormat(VkFormat format)
    {
        switch (format) {
        break;case VK_FORMAT_UNDEFINED:           return Format::Undefined;
        break;case VK_FORMAT_R8G8B8A8_UNORM:      return Format::RGBA8_UNorm;
        break;case VK_FORMAT_R8G8B8A8_SRGB:       return Format::RGBA8_SRGB;
        break;case VK_FORMAT_R16G16B16A16_SFLOAT: return Format::RGBA16_SFloat;
        break;case VK_FORMAT_R32G32B32A32_SFLOAT: return Format::RGBA32_SFloat;
        break;case VK_FORMAT_B8G8R8A8_UNORM:      return Format::BGRA8_UNorm;
        break;case VK_FORMAT_B8G8R8A8_SRGB:       return Format::BGRA8_SRGB;
        break;case VK_FORMAT_R32G32B32_SFLOAT:    return Format::RGB32_SFloat;
        break;case VK_FORMAT_R8_UNORM:            return Format::R8_UNorm;
        break;case VK_FORMAT_R8G8_UNORM:          return Format::RG8_UNorm;
        break;case VK_FORMAT_R32_SFLOAT:          return Format::R32_SFloat;
        break;case VK_FORMAT_R8_UINT:             return Format::R8_UInt;
        break;case VK_FORMAT_R16_UINT:            return Format::R16_UInt;
        break;case VK_FORMAT_R32_UINT:            return Format::R32_UInt;
        break;case VK_FORMAT_X8_D24_UNORM_PACK32: return Format::D24_UNorm;
        break;case VK_FORMAT_D24_UNORM_S8_UINT:   return Format::S8_D24_UNorm;
        break;case VK_FORMAT_D32_SFLOAT:          return Format::D32_SFloat;
        break;case VK_FORMAT_BC7_SRGB_BLOCK:      return Format::BC7_SRGB;
        break;case VK_FORMAT_BC7_UNORM_BLOCK:     return Format::BC7_Unorm;
        }

        NOVA_THROW("Unknown VkFormat: {}", u32(format));
    }

    VkIndexType GetVulkanIndexType(IndexType type)
    {
        switch (type) {
        break;case IndexType::U16: return VK_INDEX_TYPE_UINT16;
        break;case IndexType::U32: return VK_INDEX_TYPE_UINT32;
        break;case IndexType::U8:  return VK_INDEX_TYPE_UINT8_EXT;
        }

        NOVA_THROW("Unknown IndexType: {}", u32(type));
    }

    VkFilter GetVulkanFilter(Filter filter)
    {
        switch (filter) {
        break;case Filter::Linear:  return VK_FILTER_LINEAR;
        break;case Filter::Nearest: return VK_FILTER_NEAREST;
        }

        NOVA_THROW("Unknown Filter: {}", u32(filter));
    }

    VkSamplerAddressMode GetVulkanAddressMode(AddressMode mode)
    {
        switch (mode) {
        break;case AddressMode::Repeat:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;case AddressMode::RepeatMirrored: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        break;case AddressMode::Edge:           return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;case AddressMode::Border:         return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        }

        NOVA_THROW("Unknown AddressMode: {}", u32(mode));
    }

    VkBorderColor GetVulkanBorderColor(BorderColor color)
    {
        switch (color) {
        break;case BorderColor::TransparentBlack: return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        break;case BorderColor::Black:            return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        break;case BorderColor::White:            return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        }

        NOVA_THROW("Unknown BorderColor: {}", u32(color));
    }

    VkShaderStageFlags GetVulkanShaderStage(ShaderStage in)
    {
        VkShaderStageFlags out = 0;

        if (in >= ShaderStage::Vertex)       { out |= VK_SHADER_STAGE_VERTEX_BIT; }
        if (in >= ShaderStage::TessControl)  { out |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT; }
        if (in >= ShaderStage::TessEval)     { out |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT; }
        if (in >= ShaderStage::Geometry)     { out |= VK_SHADER_STAGE_GEOMETRY_BIT; }
        if (in >= ShaderStage::Fragment)     { out |= VK_SHADER_STAGE_FRAGMENT_BIT; }
        if (in >= ShaderStage::Compute)      { out |= VK_SHADER_STAGE_COMPUTE_BIT; }
        if (in >= ShaderStage::RayGen)       { out |= VK_SHADER_STAGE_RAYGEN_BIT_KHR; }
        if (in >= ShaderStage::AnyHit)       { out |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR; }
        if (in >= ShaderStage::ClosestHit)   { out |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR; }
        if (in >= ShaderStage::Miss)         { out |= VK_SHADER_STAGE_MISS_BIT_KHR; }
        if (in >= ShaderStage::Intersection) { out |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR; }
        if (in >= ShaderStage::Task)         { out |= VK_SHADER_STAGE_TASK_BIT_EXT; }
        if (in >= ShaderStage::Mesh)         { out |= VK_SHADER_STAGE_MESH_BIT_EXT; }

        return out;
    }

    VkPresentModeKHR GetVulkanPresentMode(PresentMode mode)
    {
        switch (mode) {
        break;case PresentMode::Immediate:   return VK_PRESENT_MODE_IMMEDIATE_KHR;
        break;case PresentMode::Mailbox:     return VK_PRESENT_MODE_MAILBOX_KHR;
        break;case PresentMode::Fifo:        return VK_PRESENT_MODE_FIFO_KHR;
        break;case PresentMode::FifoRelaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        }

        NOVA_THROW("Unknown PresentMode: {}", u32(mode));
    }

    VkPipelineBindPoint GetVulkanPipelineBindPoint(BindPoint point)
    {
        switch (point) {
        break;case BindPoint::Graphics:   return VK_PIPELINE_BIND_POINT_GRAPHICS;
        break;case BindPoint::Compute:    return VK_PIPELINE_BIND_POINT_COMPUTE;
        break;case BindPoint::RayTracing: return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
        }

        NOVA_THROW("Unknown BindPoint: {}", u32(point));
    }

    VkAccelerationStructureTypeKHR GetVulkanAccelStructureType(AccelerationStructureType type)
    {
        switch (type) {
        break;case AccelerationStructureType::BottomLevel: return VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        break;case AccelerationStructureType::TopLevel:    return VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        }

        NOVA_THROW("Unknown AccelerationStructureType: {}", u32(type));
    }

    VkBuildAccelerationStructureFlagsKHR GetVulkanAccelStructureBuildFlags(AccelerationStructureFlags in)
    {
        VkBuildAccelerationStructureFlagsKHR out = 0;

        if (in >= AccelerationStructureFlags::PreferFastTrace) { out |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR; }
        if (in >= AccelerationStructureFlags::PreferFastBuild) { out |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR; }
        if (in >= AccelerationStructureFlags::AllowDataAccess) { out |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR; }
        if (in >= AccelerationStructureFlags::AllowCompaction) { out |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR; }

        return out;
    }

    VkDescriptorType GetVulkanDescriptorType(DescriptorType type)
    {
        switch (type) {
        break;case DescriptorType::SampledTexture:        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        break;case DescriptorType::StorageTexture:        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        break;case DescriptorType::Uniform:               return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        break;case DescriptorType::Storage:               return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        break;case DescriptorType::AccelerationStructure: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        }

        NOVA_THROW("Unknown DescriptorType: {}", u32(type));
    }

    VkCompareOp GetVulkanCompareOp(CompareOp op)
    {
        switch (op) {
        break;case CompareOp::Never:          return VK_COMPARE_OP_NEVER;
        break;case CompareOp::Less:           return VK_COMPARE_OP_LESS;
        break;case CompareOp::Equal:          return VK_COMPARE_OP_EQUAL;
        break;case CompareOp::LessOrEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        break;case CompareOp::Greater:        return VK_COMPARE_OP_GREATER;
        break;case CompareOp::NotEqual:       return VK_COMPARE_OP_NOT_EQUAL;
        break;case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        break;case CompareOp::Always:         return VK_COMPARE_OP_ALWAYS;
        }

        NOVA_THROW("Unknown CompareOp: {}", u32(op));
    }

    VkCullModeFlags GetVulkanCullMode(CullMode in)
    {
        VkCullModeFlags out = 0;

        if (in >= CullMode::Front) { out |= VK_CULL_MODE_FRONT_BIT; }
        if (in >= CullMode::Back)  { out |= VK_CULL_MODE_BACK_BIT; }

        return out;
    }

    VkFrontFace GetVulkanFrontFace(FrontFace face)
    {
        switch (face) {
        break;case FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        break;case FrontFace::Clockwise:        return VK_FRONT_FACE_CLOCKWISE;
        }

        NOVA_THROW("Unknown FrontFace: {}", u32(face));
    }

    VkPolygonMode GetVulkanPolygonMode(PolygonMode mode)
    {
        switch (mode) {
        break;case PolygonMode::Fill:  return VK_POLYGON_MODE_FILL;
        break;case PolygonMode::Line:  return VK_POLYGON_MODE_LINE;
        break;case PolygonMode::Point: return VK_POLYGON_MODE_POINT;
        }

        NOVA_THROW("Unknown PolygonMode: {}", u32(mode));
    }

    VkPrimitiveTopology GetVulkanTopology(Topology topology)
    {
        switch (topology) {
        break;case Topology::Points:        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        break;case Topology::Lines:         return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        break;case Topology::LineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        break;case Topology::Triangles:     return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;case Topology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        break;case Topology::TriangleFan:   return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        break;case Topology::Patches:       return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        }

        NOVA_THROW("Unknown Topology: {}", u32(topology));
    }

    VkQueueFlags GetVulkanQueueFlags(QueueFlags in)
    {
        VkQueueFlags out = 0;

        if (in >= QueueFlags::Graphics) { out |= VK_QUEUE_GRAPHICS_BIT; }
        if (in >= QueueFlags::Compute)  { out |= VK_QUEUE_COMPUTE_BIT; }
        if (in >= QueueFlags::Transfer) { out |= VK_QUEUE_TRANSFER_BIT; }

        return out;
    };

    VkPipelineStageFlags2 GetVulkanPipelineStage(PipelineStage in)
    {
        VkPipelineStageFlags2 out = 0;

        if (in >= PipelineStage::Transfer)   { out |= VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT; }
        if (in >= PipelineStage::Graphics)   { out |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT; }
        if (in >= PipelineStage::RayTracing) { out |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR; }
        if (in >= PipelineStage::Compute)    { out |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT; }
        if (in >= PipelineStage::AccelBuild) { out |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR; }
        if (in >= PipelineStage::All)        { out |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; }

        return out;
    };

    VkImageLayout GetVulkanImageLayout(TextureLayout layout)
    {
        switch (layout) {
        break;case TextureLayout::Sampled:                return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        break;case TextureLayout::GeneralImage:           return VK_IMAGE_LAYOUT_GENERAL;
        break;case TextureLayout::ColorAttachment:        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        break;case TextureLayout::DepthStencilAttachment: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        break;case TextureLayout::Present:                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        NOVA_THROW("Unknown TextureLayout: {}", u32(layout));
    }
}