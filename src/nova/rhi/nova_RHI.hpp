#pragma once

#include <nova/core/nova_Core.hpp>

// #define NOVA_NOISY_VULKAN_ALLOCATIONS

namespace nova
{
    inline
    void VkCall(VkResult res)
    {
        if (res != VK_SUCCESS)
            NOVA_THROW("Error: {}", int(res));
    }

    template<class Container, class Fn, class ... Args>
    void VkQuery(Container&& container, Fn&& fn, Args&& ... args)
    {
        u32 count;
        fn(std::forward<Args>(args)..., &count, nullptr);
        container.resize(count);
        fn(std::forward<Args>(args)..., &count, container.data());
    }

// -----------------------------------------------------------------------------

    #define NOVA_DECL_DEVICE_PROC(name)  inline PFN_##name name
    #define NOVA_LOAD_DEVICE_PROC(name, device) ::nova::name = (PFN_##name)vkGetDeviceProcAddr(device, #name);\
        NOVA_LOG("Loaded fn [" #name "] - {}", (void*)name)

// -----------------------------------------------------------------------------

    struct Image;
    struct Buffer;
    struct Shader;
    struct Context;
    struct Swapchain;
    struct CommandPool;
    struct CommandList;
    struct Context;
    struct Fence;
    struct ResourceTracker;
    struct Queue;

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

    enum class BufferUsage : u64
    {
        TransferSrc = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        TransferDst = VK_BUFFER_USAGE_TRANSFER_DST_BIT,

        Uniform = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        Storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,

        Index = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        Vertex = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,

        Indirect = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,

        ShaderBindingTable = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,

        AccelBuild = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        AccelStorage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,

        DescriptorSamplers = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
        DescriptorResources = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
    };
    NOVA_DECORATE_FLAG_ENUM(BufferUsage)

    struct Buffer
    {
        Context* context = {};

        VkBuffer          buffer = {};
        VmaAllocation allocation = {};
        VkDeviceSize        size = 0ull;
        VkDeviceAddress  address = 0ull;
        b8*               mapped = nullptr;
        BufferFlags        flags = BufferFlags::None;

    public:
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
        TransferSrc        = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        TransferDst        = VK_IMAGE_USAGE_TRANSFER_DST_BIT,

        Sampled            = VK_IMAGE_USAGE_SAMPLED_BIT,
        Storage            = VK_IMAGE_USAGE_STORAGE_BIT,

        ColorAttach        = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        DepthStencilAttach = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    };
    NOVA_DECORATE_FLAG_ENUM(ImageUsage)

    enum class Format
    {
        Undefined = VK_FORMAT_UNDEFINED,

        RGBA8U = VK_FORMAT_R8G8B8A8_UNORM,
        RGBA16F = VK_FORMAT_R16G16B16A16_SFLOAT,
        RGBA32F = VK_FORMAT_R32G32B32A32_SFLOAT,

        R8U = VK_FORMAT_R8_UNORM,
        R32F = VK_FORMAT_R32_SFLOAT,

        D24U_X8 = VK_FORMAT_X8_D24_UNORM_PACK32,
        D24U_S8 = VK_FORMAT_D24_UNORM_S8_UINT,
    };

    struct Image
    {
        Context* context = {};

        VkImage             image = {};
        VmaAllocation  allocation = {};
        VkImageView          view = {};
        VkFormat           format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

        Vec3U extent = {};
        u32     mips = 0;
        u32   layers = 0;
    };


// -----------------------------------------------------------------------------

    enum class ShaderStage : uint16_t
    {
        None = 0,

        Vertex = VK_SHADER_STAGE_VERTEX_BIT,
        TessControl = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        TessEval = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        Geometry = VK_SHADER_STAGE_GEOMETRY_BIT,
        Fragment = VK_SHADER_STAGE_FRAGMENT_BIT,

        Compute = VK_SHADER_STAGE_COMPUTE_BIT,

        RayGen = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        AnyHit = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        ClosestHit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        Miss = VK_SHADER_STAGE_MISS_BIT_KHR,
        Intersection = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
    };
    NOVA_DECORATE_FLAG_ENUM(ShaderStage)

    struct Shader
    {
        Context* context = {};

        VkShaderStageFlagBits stage = VkShaderStageFlagBits(0);
        VkShaderEXT          shader = {};

        VkPipelineShaderStageCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .module = {},
            .pName = "main",
        };
    };

// -----------------------------------------------------------------------------

    enum class PresentMode : u32
    {
        Immediate   = VK_PRESENT_MODE_IMMEDIATE_KHR,
        Mailbox     = VK_PRESENT_MODE_MAILBOX_KHR,
        Fifo        = VK_PRESENT_MODE_FIFO_KHR,
        FifoRelaxed = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
    };

    struct Swapchain
    {
        Context* context = {};

        VkSurfaceKHR           surface = nullptr;
        VkSwapchainKHR       swapchain = nullptr;
        VkSurfaceFormatKHR      format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        VkImageUsageFlags        usage = 0;
        VkPresentModeKHR   presentMode = VK_PRESENT_MODE_FIFO_KHR;
        std::vector<Image*>     images = {};
        uint32_t                 index = UINT32_MAX;
        Image*                   image = nullptr;
        VkExtent2D              extent = { 0, 0 };
        bool                   invalid = false;

        std::vector<VkSemaphore> semaphores = {};
        u32                  semaphoreIndex = 0;
    };

// -----------------------------------------------------------------------------

    struct Queue
    {
        Context* context = {};

        VkQueue handle = {};
        u32     family = UINT32_MAX;

    public:
        void Submit(Span<CommandList*> commandLists, Span<Fence*> waits, Span<Fence*> signals);
        bool Acquire(Span<Swapchain*> swapchains, Span<Fence*> signals);

        // Present a set of swapchains, waiting on a number of fences.
        // If any wait dependency includes a wait-before-signal operation
        // (including indirectly) then hostWait must be set to true, as WSI
        // operations are incompatible with wait-before signal.
        void Present(Span<Swapchain*> swapchains, Span<Fence*> waits, bool hostWait = false);
    };

// -----------------------------------------------------------------------------

    struct ResourceTracker
    {
        Context* context = {};

        u64 version = 0;

        struct ImageState
        {
            VkImageLayout        layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2       access = 0;

            u64 version = 0;
        };
        ankerl::unordered_dense::map<VkImage, ImageState> imageStates;
        std::vector<VkImage> clearList;

    public:
        void Clear(u32 maxAge);

        void Reset(Image* image);
        void Persist(Image* image);
        void Set(Image* image, VkImageLayout layout, VkPipelineStageFlags2 stage, VkAccessFlags2 access);

        ImageState& Get(Image* image);
    };

// -----------------------------------------------------------------------------

    struct Fence
    {
        Context*  context = {};

        VkSemaphore semaphore = {};
        u64             value = 0;

    public:
        void Wait(u64 waitValue = 0ull);
        u64 Advance();
        void Signal(u64 value);
    };

// -----------------------------------------------------------------------------

    struct RenderingDescription
    {
        Span<Format> colorFormats;
        Format depthFormat = {};
        Format stencilFormat = {};
    };

    struct CommandPool
    {
        Context* context = {};
        Queue*     queue = {};

        VkCommandPool                pool = {};
        std::vector<CommandList*> buffers = {};
        u32                         index = 0;

        std::vector<CommandList*> secondaryBuffers = {};
        u32                         secondaryIndex = 0;

    public:
        CommandList* BeginPrimary(ResourceTracker* tracker);

        CommandList* BeginSecondary(ResourceTracker* tracker, const RenderingDescription* renderingDescription = nullptr);
        CommandList* BeginSecondary(ResourceTracker* tracker, const RenderingDescription& renderingDescription)
        {
            return BeginSecondary(tracker, &renderingDescription);
        }

        void Reset();
    };

    struct CommandList
    {
        CommandPool*        pool = {};
        ResourceTracker* tracker = {};
        VkCommandBuffer   buffer = {};

    public:
        void End();

        void UpdateBuffer(Buffer* dst, const void* pData, usz size, u64 dstOffset = 0);
        void CopyToBuffer(Buffer* dst, Buffer* src, u64 size, u64 dstOffset = 0, u64 srcOffset = 0);
        void CopyToImage(Image* dst, Buffer* src, u64 srcOffset = 0);
        void GenerateMips(Image* image);

        void Clear(Image* image, Vec4 color);
        void Transition(Image* image, VkImageLayout newLayout, VkPipelineStageFlags2 newStages, VkAccessFlags2 newAccess);

        void SetViewport(Vec2U size, bool flipVertical);
        void SetTopology(VkPrimitiveTopology topology);
        void SetBlendState(u32 colorAttachmentCount, bool blendEnable);
        // TODO: Depth/stencil attachments
        void BeginRendering(Span<Image*> colorAttachments, Span<Vec4> clearColors, bool allowSecondary = false);
        void EndRendering();
        void BindShaders(Span<Shader*> shaders);
        void ClearAttachment(u32 attachment, Vec4 color, Vec2U size, Vec2I offset = {});
        void PushConstants(VkPipelineLayout layout, ShaderStage stages, u64 offset, u64 size, const void* data);
        void Draw(u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance);
        void ExecuteCommands(Span<CommandList*> commands);
    };

// -----------------------------------------------------------------------------

    struct ContextConfig
    {
        bool debug = false;
        bool rayTracing = false;
    };

    struct Context
    {
        VkInstance  instance = {};
        VkPhysicalDevice gpu = {};
        VkDevice      device = {};
        VmaAllocator     vma = {};

        VkDebugUtilsMessengerEXT debugMessenger = {};

        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void* userData);

        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorSizes = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
        };

        Queue* graphics = {};

        static std::atomic_int64_t    AllocationCount;
        static std::atomic_int64_t NewAllocationCount;

        VkAllocationCallbacks alloc = {
            .pfnAllocation = +[](void*, size_t size, size_t align, [[maybe_unused]] VkSystemAllocationScope scope) {
                void* ptr = mi_malloc_aligned(size, align);
                if (ptr)
                {
                    ++AllocationCount;
                    ++NewAllocationCount;
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                    NOVA_LOG(" --\n{}", std::stacktrace::current());
                    NOVA_LOG("Allocating size = {}, align = {}, scope = {}, ptr = {}", size, align, int(scope), ptr);
#endif
                }
                return ptr;
            },
            .pfnReallocation = +[](void*, void* orig, size_t size, size_t align, VkSystemAllocationScope) {
                void* ptr = mi_realloc_aligned(orig, size, align);
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                NOVA_LOG("Reallocated, size = {}, align = {}, ptr = {} -> {}", size, align, orig, ptr);
#endif
                return ptr;
            },
            .pfnFree = +[](void*, void* ptr) {
                if (ptr)
                {
                    --AllocationCount;
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                    NOVA_LOG("Freeing ptr = {}", ptr);
                    NOVA_LOG("    Allocations - :: {}", AllocationCount.load());
#endif
                }
                mi_free(ptr);
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
        static Context* Create(const ContextConfig& config);
        static void Destroy(Context* context);

        Buffer* CreateBuffer(u64 size, BufferUsage usage, BufferFlags flags = {});
        void DestroyBuffer(Buffer* buffer);

        Image* CreateImage(Vec3U size, ImageUsage usage, Format format, ImageFlags flags = {});
        void DestroyImage(Image* image);

        Shader* CreateShader(ShaderStage stage, ShaderStage nextStage,
            const std::string& filename, const std::string& sourceCode = {},
            Span<VkPushConstantRange> pushConstantRanges = {},
            Span<VkDescriptorSetLayout> descriptorSetLayouts = {});
        void DestroyShader(Shader* shader);

        Swapchain* CreateSwapchain(VkSurfaceKHR surface, ImageUsage usage, PresentMode presentMode);
        void DestroySwapchain(Swapchain* swapchain);

        VkSurfaceKHR CreateSurface(HWND hwnd);
        void DestroySurface(VkSurfaceKHR surface);

        CommandPool* CreateCommandPool();
        void DestroyCommandPool(CommandPool* commands);

        ResourceTracker* CreateResourceTracker();
        void DestroyResourceTracker(ResourceTracker* tracker);

        Fence* CreateFence();
        void DestroyFence(Fence* fence);

        void WaitForIdle();
    };
}
