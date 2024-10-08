#pragma once

#include <nova/gpu/RHI.hpp>

#ifndef VK_NO_PROTOTYPES
#  define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <shared_mutex>
#include <bitset>
#include <deque>

namespace nova
{
    namespace vkh
    {
        inline
        void Check(VkResult res, const std::source_location& loc = std::source_location::current())
        {
            if (res != VK_SUCCESS) {
                throw Exception(Fmt("Error: {}", int(res)), loc);
            }
        }

        template<typename VulkanType, typename Fn, typename ...Args>
        Span<VulkanType> StackEnumerate(Fn&& fn, Args&&... args)
        {
            auto& stack = detail::GetThreadStack();
            VulkanType* begin = reinterpret_cast<VulkanType*>(stack.ptr);
            u32 count = UINT_MAX;
            fn(args..., &count, begin);
            stack.ptr = AlignUpPower2(reinterpret_cast<std::byte*>(begin + count), 16);
            return { begin, count };
        }

#define NOVA_STACK_VKH_ENUMERATE(vk_type, fn, ...) nova::vkh::StackEnumerate<vk_type>(fn __VA_OPT__(,) __VA_ARGS__)
    }

// -----------------------------------------------------------------------------

    struct VulkanFormat
    {
        Format      format;
        VkFormat vk_format;

        u32    atom_size = 0;
        u32  block_width = 1;
        u32 block_height = 1;
    };

    enum class UID : u64
    {
        Invalid = 0,
    };

    template<>
    struct Handle<Queue>::Impl
    {
        Context context = {};

        VkQueueFlags           flags = {};
        VkQueue               handle = {};
        u32                   family = UINT32_MAX;
        VkPipelineStageFlags2 stages = {};

        Fence fence;

        struct CommandPool;

        struct SubmittedCommandList
        {
            CommandList command_list;
            u64          fence_value;
        };

        struct CommandPool
        {
            VkCommandPool                       command_pool;
            std::vector<CommandList> available_command_lists;
        };

        std::vector<CommandPool*>                        pools;
        std::deque<SubmittedCommandList> pending_command_lists;

        CommandPool* AcquireCommandPool();
        void DestroyCommandPools();
        void ClearPendingCommandLists();
        void ReleaseCommandPoolForList(CommandList);
        void MoveCommandListToPending(CommandList);
    };

    template<>
    struct Handle<Fence>::Impl
    {
        Context                       context = {};
        VkSemaphore                 semaphore = {};
        std::atomic<u64> last_submitted_value = 0;
        std::atomic<u64>      last_seen_value = 0;
    };

    template<>
    struct Handle<CommandList>::Impl
    {
        Context                        context = {};
        Queue                            queue = {};
        Queue::Impl::CommandPool* command_pool = {};
        bool                         recording = false;
        VkCommandBuffer                 buffer = {};

        // Graphics Pipeline Library fallback

        std::vector<Format> color_attachments_formats;
        Format                depth_attachment_format = nova::Format::Undefined;
        Format              stencil_attachment_format = nova::Format::Undefined;

        u32 layers;
        u32 view_mask;

        PolygonMode polygon_mode;
        Topology        topology;

        std::bitset<8>  blend_states;
        std::vector<HShader> shaders;

        VkPipeline bound_graphics_pipeline = nullptr;

        bool using_shader_objects = false;
        bool graphics_state_dirty = false;

        void EnsureGraphicsState();
        void Transition(HImage, VkImageLayout, VkPipelineStageFlags2);
    };

    struct DescriptorHeap
    {
        Context context = {};

        VkDescriptorSetLayout heap_layout = {};
        VkPipelineLayout  pipeline_layout = {};

        VkDescriptorPool  descriptor_pool = {};
        VkDescriptorSet    descriptor_set = {};

        Buffer          descriptor_buffer = {};

        u32        image_descriptor_count = 0;
        u32      sampler_descriptor_count = 0;

        u64 sampled_offset, sampled_stride;
        u64 storage_offset, storage_stride;
        u64 sampler_offset, sampler_stride;

        IndexFreeList   image_handles;
        IndexFreeList sampler_handles;

        std::shared_mutex mutex;

        void Init(HContext context, u32 image_descriptor_count, u32 sampler_descriptor_count);
        void Destroy();

        void Bind(CommandList cmd, BindPoint bind_point);

        void WriteStorage(u32 index, HImage image);
        void WriteSampled(u32 index, HImage image);
        void WriteSampler(u32 index, HSampler sampler);
    };

    struct TransferManager
    {
        Context context = {};

        Queue          queue = {};
        Buffer       staging = {};

        std::shared_mutex mutex;

        bool staged_image_copy = true;

        void Init(HContext context);
        void Destroy();
    };

    struct SwapchainStrategy
    {
    protected:
        ~SwapchainStrategy() = default;

    public:
        virtual void Destroy(Swapchain impl) = 0;

        virtual Image Target(Swapchain impl) = 0;
        virtual Vec2U Extent(Swapchain impl) = 0;
        virtual Format Format(Swapchain impl) = 0;

        virtual SyncPoint Acquire(Queue impl, Span<HSwapchain> swapchains, bool* any_resized) = 0;
        virtual void Present(Queue impl, Span<HSwapchain> swapchains, Span<SyncPoint> waits, PresentFlag flags) = 0;
        virtual void PreparePresent(CommandList cmd, HSwapchain swapchain) = 0;
    };

    template<>
    struct Handle<Swapchain>::Impl
    {
        SwapchainStrategy* strategy;
    };

    template<>
    struct Handle<Shader>::Impl
    {
        Context context = {};

        UID id = UID::Invalid;

        std::string entry = {};
        ShaderStage stage = {};

        std::vector<u32> code;

        VkShaderModuleCreateInfo create_info = {};
        VkShaderModule                handle = {};
        VkShaderEXT                   shader = {};

    public:
        VkPipelineShaderStageCreateInfo GetStageInfo() const;
    };

    template<>
    struct Handle<Buffer>::Impl
    {
        Context context = {};

        VkBuffer          buffer = {};
        union {
            VmaAllocation allocation = {};
            VkDeviceMemory  imported;
        };
        VkDeviceSize       size = 0ull;
        VkDeviceAddress address = 0ull;
        void*      host_address = 0ull;
        BufferFlags       flags = BufferFlags::None;
        BufferUsage       usage = {};
    };

    template<>
    struct Handle<Sampler>::Impl
    {
        Context context = {};

        VkSampler sampler;

        u32 descriptor_index;
    };

    template<>
    struct Handle<Image>::Impl
    {
        Context context = {};

        VkImage             image = {};
        VmaAllocation  allocation = {};
        VkImageView          view = {};
        ImageUsage        usage = {};
        Format             format = Format::Undefined;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

        VkImageLayout        layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;

        u32 descriptor_index = UINT_MAX;

        Vec3U extent = {};
        u32     mips = 0;
        u32   layers = 0;
    };

    template<>
    struct Handle<AccelerationStructureBuilder>::Impl
    {
        Context context = {};

        VkAccelerationStructureTypeKHR        type = {};
        VkBuildAccelerationStructureFlagsKHR flags = {};

        u64          build_size = 0;
        u64  build_scratch_size = 0;
        u64 update_scratch_size = 0;

        std::vector<VkAccelerationStructureGeometryKHR>   geometries;
        std::vector<u32>                            primitive_counts;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;

        u32 geometry_count = 0;
        u32 first_geometry = 0;
        bool    size_dirty = false;

        VkQueryPool query_pool = {};
    };

    template<>
    struct Handle<AccelerationStructure>::Impl
    {
        Context context = {};

        VkAccelerationStructureKHR structure = {};
        u64                          address = {};
        VkAccelerationStructureTypeKHR  type = {};

        Buffer   buffer;
        bool own_buffer;
    };

    template<>
    struct Handle<RayTracingPipeline>::Impl
    {
        Context context = {};

        VkPipeline pipeline = {};
        Buffer   sbt_buffer = {};

        u32         handle_size;
        u32       handle_stride;
        std::vector<u8> handles;

        VkStridedDeviceAddressRegionKHR  raygen_region = {};
        VkStridedDeviceAddressRegionKHR raymiss_region = {};
        VkStridedDeviceAddressRegionKHR  rayhit_region = {};
        VkStridedDeviceAddressRegionKHR raycall_region = {};
    };

    // -----------------------------------------------------------------------------

    struct GraphicsPipelineKey
    {
        std::array<UID, 5>              shaders;
        Topology                       topology;
        PolygonMode                   poly_mode;
        std::array<Format, 8> color_attachments;
        Format                 depth_attachment;
        Format               stencil_attachment;
        u32                           view_mask;
        std::bitset<8>             blend_states;

        NOVA_MEMORY_EQUALITY_MEMBER(GraphicsPipelineKey)
    };

    struct GraphicsPipelineVertexInputStageKey
    {
        Topology topology;

        NOVA_MEMORY_EQUALITY_MEMBER(GraphicsPipelineVertexInputStageKey)
    };

    struct GraphicsPipelinePreRasterizationStageKey
    {
        std::array<UID, 4> shaders;
        PolygonMode      poly_mode;
        u32              view_mask;

        NOVA_MEMORY_EQUALITY_MEMBER(GraphicsPipelinePreRasterizationStageKey)
    };

    struct GraphicsPipelineFragmentShaderStageKey
    {
        UID    shader;
        u32 view_mask;

        NOVA_MEMORY_EQUALITY_MEMBER(GraphicsPipelineFragmentShaderStageKey)
    };

    struct GraphicsPipelineFragmentOutputStageKey
    {
        std::array<Format, 8> color_attachments;
        Format                 depth_attachment;
        Format               stencil_attachment;
        u32                           view_mask;
        std::bitset<8>             blend_states;

        NOVA_MEMORY_EQUALITY_MEMBER(GraphicsPipelineFragmentOutputStageKey)
    };

    struct ComputePipelineKey
    {
        UID shader;

        NOVA_MEMORY_EQUALITY_MEMBER(ComputePipelineKey)
    };
}

NOVA_MEMORY_HASH(nova::GraphicsPipelineKey);
NOVA_MEMORY_HASH(nova::GraphicsPipelineVertexInputStageKey);
NOVA_MEMORY_HASH(nova::GraphicsPipelinePreRasterizationStageKey);
NOVA_MEMORY_HASH(nova::GraphicsPipelineFragmentShaderStageKey);
NOVA_MEMORY_HASH(nova::GraphicsPipelineFragmentOutputStageKey);
NOVA_MEMORY_HASH(nova::ComputePipelineKey);

namespace nova
{

// -----------------------------------------------------------------------------

    VkBufferUsageFlags GetVulkanBufferUsage(BufferUsage usage);
    VkImageUsageFlags GetVulkanImageUsage(ImageUsage usage);
    const VulkanFormat& GetVulkanFormat(Format format);
    Format FromVulkanFormat(VkFormat format);
    VkIndexType GetVulkanIndexType(IndexType type);
    VkFilter GetVulkanFilter(Filter filter);
    VkSamplerAddressMode GetVulkanAddressMode(AddressMode mode);
    VkBorderColor GetVulkanBorderColor(BorderColor color);
    VkShaderStageFlags GetVulkanShaderStage(ShaderStage in);
    VkPresentModeKHR GetVulkanPresentMode(PresentMode mode);
    VkPipelineBindPoint GetVulkanPipelineBindPoint(BindPoint point);
    VkAccelerationStructureTypeKHR GetVulkanAccelStructureType(AccelerationStructureType type);
    VkBuildAccelerationStructureFlagsKHR GetVulkanAccelStructureBuildFlags(AccelerationStructureFlags in);
    VkDescriptorType GetVulkanDescriptorType(DescriptorType type);
    VkCompareOp GetVulkanCompareOp(CompareOp op);
    VkCullModeFlags GetVulkanCullMode(CullMode in);
    VkFrontFace GetVulkanFrontFace(FrontFace face);
    VkPolygonMode GetVulkanPolygonMode(PolygonMode mode);
    VkPrimitiveTopology GetVulkanTopology(Topology topology);
    VkQueueFlags GetVulkanQueueFlags(QueueFlags in);
    VkPipelineStageFlags2 GetVulkanPipelineStage(PipelineStage in);
    VkImageLayout GetVulkanImageLayout(ImageLayout layout);

    PFN_vkGetInstanceProcAddr Platform_LoadGetInstanceProcAddr();
    VkSurfaceKHR Platform_CreateVulkanSurface(Context, void* handle);
    bool Platform_GpuSupportsPresent(Context, VkPhysicalDevice handle);
    void Platform_AddPlatformExtensions(std::vector<const char*>& extensions);

    using Vulkan_ShaderCompilerFuncPtr = std::vector<u32>(*)(ShaderStage /* stage */, StringView /* entry */, StringView /* filename */, Span<StringView> /* fragments */);
    std::monostate Vulkan_RegisterCompiler(ShaderLang lang, Vulkan_ShaderCompilerFuncPtr fptr);

    void* Vulkan_TrackedAllocate(  void* userdata,                 size_t size, size_t alignment,         VkSystemAllocationScope allocation_scope);
    void* Vulkan_TrackedReallocate(void* userdata, void* original, size_t size, size_t alignment,         VkSystemAllocationScope allocation_scope);
    void  Vulkan_TrackedFree(      void* userdata, void* memory);
    void  Vulkan_NotifyAllocation( void* userdata,                 size_t size, VkInternalAllocationType, VkSystemAllocationScope);
    void  Vulkan_NotifyFree(       void* userdata,                 size_t size, VkInternalAllocationType, VkSystemAllocationScope);

// -----------------------------------------------------------------------------
//                                 Context
// -----------------------------------------------------------------------------

    void VulkanFunctions_Init(Handle<Context>::Impl* vk_functions, PFN_vkGetInstanceProcAddr loader);
    void VulkanFunctions_LoadInstance(Handle<Context>::Impl* vk_functions, VkInstance instance);
    void VulkanFunctions_LoadDevice(Handle<Context>::Impl* vk_functions, VkDevice device);

    template<>
    struct Handle<Context>::Impl
    {
#define NOVA_VULKAN_FUNCTION(vk_function, ...) PFN_##vk_function vk_function;
#include "VulkanFunctions.inl"
#undef NOVA_VULKAN_FUNCTION

        ContextConfig config = {};

        VkInstance  instance = {};
        VkPhysicalDevice gpu = {};
        VkDevice      device = {};
        VmaAllocator     vma = {};

        VkDebugUtilsMessengerEXT debug_messenger = {};

        DescriptorHeap       global_heap;
        TransferManager transfer_manager;

        std::vector<Queue> graphics_queues = {};
        std::vector<Queue> transfer_queues = {};
        std::vector<Queue>  compute_queues = {};
        u32             queue_family_count = 0;
        std::array<u32, 3>  queue_families;

        ContextProperties properties = {
            .max_push_constant_size = 128,
            .max_multiview_count = 6,
        };

// -----------------------------------------------------------------------------
//                              Device Properties
// -----------------------------------------------------------------------------

        bool      external_host_memory = false;
        bool            shader_objects = false;
        bool graphics_pipeline_library = false;
        bool        descriptor_buffers = false;
        bool         no_shader_modules = false;
        bool             resizable_bar = false;
        bool              mesh_shading = false;

        VkPhysicalDeviceMemoryProperties memory_properties = {};

        VkPhysicalDeviceVulkan11Properties vulkan11_properties  = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,
        };

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_pipeline_properties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
            .pNext = &vulkan11_properties,
        };

        VkPhysicalDeviceAccelerationStructurePropertiesKHR accel_structure_properties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
            .pNext = &ray_tracing_pipeline_properties,
        };

        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_sizes = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
            .pNext = &accel_structure_properties,
        };

// -----------------------------------------------------------------------------
//                             Pipeline caches
// -----------------------------------------------------------------------------

        std::atomic_uint64_t next_uid = 1;
        UID GetUID() noexcept { return UID(next_uid++); };

        std::shared_mutex pipeline_cache_mutex;

        HashMap<GraphicsPipelineVertexInputStageKey, VkPipeline>       vertex_input_stages;
        HashMap<GraphicsPipelinePreRasterizationStageKey, VkPipeline>     preraster_stages;
        HashMap<GraphicsPipelineFragmentShaderStageKey, VkPipeline> fragment_shader_stages;
        HashMap<GraphicsPipelineFragmentOutputStageKey, VkPipeline> fragment_output_stages;
        HashMap<GraphicsPipelineKey, VkPipeline>                    graphics_pipeline_sets;
        HashMap<ComputePipelineKey, VkPipeline>                          compute_pipelines;
        VkPipelineCache pipeline_cache = {};

// -----------------------------------------------------------------------------
//                           Allocation Tracking
// -----------------------------------------------------------------------------

        VkAllocationCallbacks alloc_notify = {
            .pfnAllocation = Vulkan_TrackedAllocate,
            .pfnReallocation = Vulkan_TrackedReallocate,
            .pfnFree = Vulkan_TrackedFree,
            .pfnInternalAllocation = Vulkan_NotifyAllocation,
            .pfnInternalFree = Vulkan_NotifyFree,
        };
        VkAllocationCallbacks* alloc = &alloc_notify;
    };
}
