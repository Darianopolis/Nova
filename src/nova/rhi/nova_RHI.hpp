#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Ref.hpp>

// #define NOVA_NOISY_VULKAN_ALLOCATIONS

namespace nova
{
    inline
    void VkCall(VkResult res, std::source_location loc = std::source_location::current())
    {
        if (res != VK_SUCCESS)
            Error(std::format("Error: {} ({} @ {})\n", int(res), loc.line(), loc.file_name()));
    }

    #define NOVA_VKQUERY(name, func, ...) do {                \
        u32 count;                                           \
        func(__VA_ARGS__ __VA_OPT__(,) &count, nullptr);     \
        name.resize(count);                                  \
        func(__VA_ARGS__ __VA_OPT__(,) &count, name.data()); \
    } while(0)

// -----------------------------------------------------------------------------

    #define NOVA_DECL_DEVICE_PROC(name)  inline PFN_##name name
    #define NOVA_LOAD_DEVICE_PROC(name, device) ::nova::name = (PFN_##name)vkGetDeviceProcAddr(device, #name);\
        NOVA_LOG("Loaded fn [" #name "] - {}", (void*)name)

// -----------------------------------------------------------------------------

#define NOVA_DECLARE_STRUCTURE(name) \
    struct name; \
    using name##Ref = Ref<name>;

    NOVA_DECLARE_STRUCTURE(Image)
    NOVA_DECLARE_STRUCTURE(Buffer)
    NOVA_DECLARE_STRUCTURE(Shader)
    NOVA_DECLARE_STRUCTURE(Context)
    NOVA_DECLARE_STRUCTURE(Swapchain)
    NOVA_DECLARE_STRUCTURE(Commands)
    NOVA_DECLARE_STRUCTURE(Context)
    NOVA_DECLARE_STRUCTURE(Fence)
    NOVA_DECLARE_STRUCTURE(ResourceTracker)

// -----------------------------------------------------------------------------

    enum class BufferFlags
    {
        None,
        Addressable  = 1 << 0,
        DeviceLocal  = 1 << 1 | Addressable,
        Mappable     = 1 << 2,
        CreateMapped = 1 << 3 | Mappable,
    };
    NOVA_DECORATE_FLAG_ENUM(BufferFlags)

    enum class BufferUsage
    {
        Uniform = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        Storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        AccelBuild = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        AccelStorage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
        Indirect = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        Vertex = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        Index = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        DescriptorSamplers = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
        DescriptorResources = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
    };
    NOVA_DECORATE_FLAG_ENUM(BufferUsage)

    struct Buffer : RefCounted
    {
        ContextRef context = {};

        VkBuffer          buffer = {};
        VmaAllocation allocation = {};
        VkDeviceSize        size = 0ull;
        VkDeviceAddress  address = 0ull;
        b8*               mapped = nullptr;
        BufferFlags        flags = BufferFlags::None;

    public:
        ~Buffer();

        template<class T>
        T& Get(u64 index, u64 offset = 0)
        {
            return reinterpret_cast<T*>(mapped + offset)[index];
        }
    };


// -----------------------------------------------------------------------------

    enum class ImageFlags
    {
        Array = 1 << 0,
        Mips  = 1 << 1,
    };
    NOVA_DECORATE_FLAG_ENUM(ImageFlags)

    enum class ImageUsage
    {
        Sampled    = 1 << 0,
        Attachment = 1 << 1,
        Storage    = 1 << 2,
    };
    NOVA_DECORATE_FLAG_ENUM(ImageUsage)

    enum class Format
    {
        RGBA8U,
        RGBA16F,
        RGBA32F,

        R8U,
        R32F,

        D24U_X8,
        D24U_S8,
    };

    struct Image : RefCounted
    {
        ContextRef context = {};

        VkImage             image = {};
        VmaAllocation  allocation = {};
        VkImageView          view = {};
        VkImageLayout      layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkFormat           format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

        Vec3U extent = {};
        u32     mips = 0;
        u32   layers = 0;

    public:
        ~Image();
    };


// -----------------------------------------------------------------------------

    struct Shader : RefCounted
    {
        ContextRef context = {};

        VkShaderStageFlagBits stage = VkShaderStageFlagBits(0);
        VkShaderEXT          shader = {};

        VkPipelineShaderStageCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .module = {},
            .pName = "main",
        };

    public:
        ~Shader();
    };

// -----------------------------------------------------------------------------

    struct Swapchain : RefCounted
    {
        ContextRef context = {};

        VkSurfaceKHR           surface = nullptr;
        VkSwapchainKHR       swapchain = nullptr;
        VkSurfaceFormatKHR      format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        VkImageUsageFlags        usage = 0;
        VkPresentModeKHR   presentMode = VK_PRESENT_MODE_FIFO_KHR;
        std::vector<ImageRef> images = {};
        uint32_t                 index = UINT32_MAX;
        Image*                   image = nullptr;
        VkExtent2D              extent = { 0, 0 };

        static constexpr u32 SemaphoreCount = 4;
        std::vector<VkSemaphore> semaphores = {};
        u32                  semaphoreIndex = 0;

    public:
        ~Swapchain();
    };

// -----------------------------------------------------------------------------

    struct Queue
    {
        VkQueue handle = {};
        u32     family = UINT32_MAX;
    };

    struct ResourceTracker : RefCounted
    {
        ContextRef context = {};

        struct ImageState
        {
            VkImageLayout        layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2       access = 0;
        };
        ankerl::unordered_dense::map<VkImage, ImageState> imageStates;

    public:
        ~ResourceTracker();
    };

    struct Fence : RefCounted
    {
        ContextRef  context = {};

        VkSemaphore semaphore = {};
        u64             value = 0;

    public:
        ~Fence();

        void Wait(u64 waitValue = 0ull);
    };

    struct Commands : RefCounted
    {
        ContextRef context = {};

        Queue* queue = {};

        VkCommandPool                   pool = {};
        std::vector<VkCommandBuffer> buffers = {};
        u32                            index = 0;

    public:
        ~Commands();

        VkCommandBuffer Allocate();

        void Clear();
        void Submit(VkCommandBuffer cmd, Fence* wait, Fence* signal);
    };

// -----------------------------------------------------------------------------

    struct Context : RefCounted
    {
        VkInstance  instance = {};
        VkPhysicalDevice gpu = {};
        VkDevice      device = {};
        VmaAllocator     vma = {};

        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorSizes = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
        };

        FenceRef fence;

        CommandsRef commands;
        VkCommandBuffer cmd;

        Queue graphics;

        BufferRef staging;
        CommandsRef transferCommands;
        VkCommandBuffer transferCmd;

        static std::atomic_int64_t AllocationCount;

        VkAllocationCallbacks alloc = {
            .pfnAllocation = +[](void*, size_t size, size_t align, [[maybe_unused]] VkSystemAllocationScope scope) {
                void* ptr = _aligned_malloc(size, align);
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                if (ptr)
                {
                    NOVA_LOG("Allocating size = {}, align = {}, scope = {}, ptr = {}", size, align, int(scope), ptr);
                    NOVA_LOG("    Allocations  + :: {}", ++AllocationCount);
                }
#endif
                return ptr;
            },
            .pfnReallocation = +[](void*, void* orig, size_t size, size_t align, VkSystemAllocationScope) {
                void* ptr = _aligned_realloc(orig, size, align);
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                NOVA_LOG("Reallocated, size = {}, align = {}, ptr = {} -> {}", size, align, orig, ptr);
#endif
                return ptr;
            },
            .pfnFree = +[](void*, void* ptr) {
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                if (ptr)
                {
                    NOVA_LOG("Freeing ptr = {}", ptr);
                    NOVA_LOG("    Allocations - :: {}", --AllocationCount);
                }
#endif
                _aligned_free(ptr);
            },
            .pfnInternalAllocation = +[](void*, size_t size, VkInternalAllocationType type, VkSystemAllocationScope) {
                NOVA_LOG("Internal allocation of size {}, type = {}", size, int(type));
            },
            .pfnInternalFree = +[](void*, size_t size, VkInternalAllocationType type, VkSystemAllocationScope) {
                NOVA_LOG("Internal free of size {}, type = {}", size, int(type));
            },
        };
        VkAllocationCallbacks* pAlloc = &alloc;
        // VkAllocationCallbacks* pAlloc = nullptr;
    public:
        ~Context();

        static ContextRef Create(bool debug);

        BufferRef CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, BufferFlags flags = {});
        void CopyToBuffer(Buffer& buffer, const void* data, size_t size, VkDeviceSize offset = 0);

        ImageRef CreateImage(Vec3U size, VkImageUsageFlags usage, VkFormat format, ImageFlags flags = {});
        void CopyToImage(Image& image, const void* data, size_t size);
        void GenerateMips(Image& image);
        void Transition(VkCommandBuffer cmd, Image& image, VkImageLayout newLayout);
        void TransitionMip(VkCommandBuffer cmd, Image& image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mip);

        ShaderRef CreateShader(VkShaderStageFlagBits stage, VkShaderStageFlags nextStage,
            const std::string& filename, const std::string& sourceCode = {},
            std::initializer_list<VkPushConstantRange> pushConstantRanges = {},
            std::initializer_list<VkDescriptorSetLayout> descriptorSetLayouts = {});

        SwapchainRef CreateSwapchain(VkSurfaceKHR surface, VkImageUsageFlags usage, VkPresentModeKHR presentMode);
        void Present(Swapchain& swapchain, Fence* wait);
        bool GetNextImage(Swapchain& swapchain, Queue& queue, Fence* signal);

        CommandsRef CreateCommands();

        FenceRef CreateFence();
    };
}