#include <nova/rhi/nova_RHI.hpp>

#include <nova/core/nova_Registry.hpp>

namespace nova
{
    struct VulkanQueue
    {
        VkQueue handle = {};
        u32     family = UINT32_MAX;
    };

    struct VulkanFence
    {
        VkSemaphore semaphore = {};
        u64             value = 0;
    };

    struct VulkanSwapchain
    {
        VkSurfaceKHR          surface = {};
        VkSwapchainKHR      swapchain = {};
        VkSurfaceFormatKHR     format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        VkImageUsageFlags       usage = 0;
        VkPresentModeKHR  presentMode = VK_PRESENT_MODE_FIFO_KHR;
        std::vector<Texture> textures = {};
        uint32_t                index = UINT32_MAX;
        VkExtent2D             extent = { 0, 0 };
        bool                  invalid = false;

        std::vector<VkSemaphore> semaphores = {};
        u32                  semaphoreIndex = 0;
    };

    struct VulkanBuffer
    {
        VkBuffer          buffer = {};
        VmaAllocation allocation = {};
        VkDeviceSize        size = 0ull;
        VkDeviceAddress  address = 0ull;
        b8*               mapped = nullptr;
        BufferFlags        flags = BufferFlags::None;
        VkBufferUsageFlags usage = {};
    };

    struct VulkanTexture
    {
        VkImage             image = {};
        VmaAllocation  allocation = {};
        VkImageView          view = {};
        VkImageUsageFlags   usage = {};
        VkFormat           format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

        Vec3U extent = {};
        u32     mips = 0;
        u32   layers = 0;
    };

    struct VulkanContext : Context
    {
        ContextConfig config;

        VkInstance  instance = {};
        VkPhysicalDevice gpu = {};
        VkDevice      device = {};
        VmaAllocator     vma = {};

        VkDebugUtilsMessengerEXT debugMessenger = {};

    public:
        VkDescriptorPool descriptorPool = {};

        Queue graphics = {};

    public:
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        };

        VkPhysicalDeviceAccelerationStructurePropertiesKHR accelStructureProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
            .pNext = &rayTracingPipelineProperties,
        };

        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorSizes = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
            .pNext = &accelStructureProperties,
        };

    public:
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

    public:
        VulkanContext(const ContextConfig& config);
        ~VulkanContext();

        void WaitIdle() override;

#define NOVA_ADD_VULKAN_REGISTRY(type, name) \
    Registry<Vulkan##type, type> name;       \
    Vulkan##type& Get(type id) { return name.Get(id); }

// -----------------------------------------------------------------------------
//                                 Queue
// -----------------------------------------------------------------------------

        NOVA_ADD_VULKAN_REGISTRY(Queue, queues)

        Queue Queue_Get(QueueFlags flags) override;
        void  Queue_Submit(Queue, Span<CommandList> commandLists, Span<Fence> signals) override;
        bool  Queue_Acquire(Queue, Span<Swapchain> swapchains, Span<Fence> signals) override;
        void  Queue_Present(Queue, Span<Swapchain> swapchains, Span<Fence> waits, bool hostWait = false) override;

// -----------------------------------------------------------------------------
//                                 Fence
// -----------------------------------------------------------------------------

        NOVA_ADD_VULKAN_REGISTRY(Fence, fences)

        Fence Fence_Create() override;
        void  Destroy(Fence) override;
        void  Fence_Wait(Fence, u64 waitValue) override;
        u64   Fence_Advance(Fence) override;
        void  Fence_Signal(Fence, u64 signalValue) override;
        u64   Fence_GetPendingValue(Fence) override;

// -----------------------------------------------------------------------------
//                               Swapchain
// -----------------------------------------------------------------------------

        NOVA_ADD_VULKAN_REGISTRY(Swapchain, swapchains)

        Swapchain Swapchain_Create(void* window, TextureUsage usage, PresentMode presentMode) override;
        void      Destroy(Swapchain) override;
        Texture   Swapchain_GetCurrent(Swapchain) override;
        Vec2U     Swapchain_GetExtent(Swapchain) override;
        Format    Swapchain_GetFormat(Swapchain) override;

// -----------------------------------------------------------------------------
//                                 Buffer
// -----------------------------------------------------------------------------

        NOVA_ADD_VULKAN_REGISTRY(Buffer, buffers)

        Buffer Buffer_Create(u64 size, BufferUsage usage, BufferFlags flags = {}) override;
        void   Destroy(Buffer) override;
        void   Buffer_Resize(Buffer, u64 size) override;
        u64    Buffer_GetSize(Buffer) override;
        b8*    Buffer_GetMapped(Buffer) override;
        u64    Buffer_GetAddress(Buffer) override;
        void*  BufferImpl_Get(Buffer, u64 index, u64 offset, usz stride) override;
        void   BufferImpl_Set(Buffer, const void* data, usz count, u64 index, u64 offset, usz stride) override;

// -----------------------------------------------------------------------------
//                                 Texture
// -----------------------------------------------------------------------------

        NOVA_ADD_VULKAN_REGISTRY(Texture, textures)

        Texture Texture_Create(Vec3U size, TextureUsage usage, Format format, TextureFlags flags) override;
        void    Destroy(Texture) override;
        Vec3U   Texture_GetExtent(Texture) override;
        Format  Texture_GetFormat(Texture) override;
    };
}