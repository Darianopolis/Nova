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

    struct Image
    {
        Context* context = {};

        VkImage             image = {};
        VmaAllocation  allocation = {};
        VkImageView          view = {};
        VkImageLayout      layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkFormat           format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

        Vec3U extent = {};
        u32     mips = 0;
        u32   layers = 0;
    };


// -----------------------------------------------------------------------------

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

        static constexpr u32 SemaphoreCount = 4;
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
        void Submit(CommandList* cmd, Fence* wait, Fence* signal);

        bool Acquire(Swapchain* swapchain, Fence* fence);
        void Present(Swapchain* swapchain, Fence* fence);
    };

// -----------------------------------------------------------------------------

    // struct ResourceTracker
    // {
    //     Context* context = {};

    //     struct ImageState
    //     {
    //         VkImageLayout        layout = VK_IMAGE_LAYOUT_UNDEFINED;
    //         VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
    //         VkAccessFlags2       access = 0;
    //     };
    //     ankerl::unordered_dense::map<VkImage, ImageState> imageStates;

    // public:
    //     ~ResourceTracker();
    // };

// -----------------------------------------------------------------------------

    struct Fence
    {
        Context*  context = {};

        VkSemaphore semaphore = {};
        u64             value = 0;

    public:
        void Wait(u64 waitValue = 0ull);
    };

// -----------------------------------------------------------------------------

    struct CommandPool
    {
        Context* context = {};
        Queue*     queue = {};

        VkCommandPool                pool = {};
        std::vector<CommandList*> buffers = {};
        u32                         index = 0;

    public:
        CommandList* Begin();
        void Reset();
    };

    struct CommandList
    {
        CommandPool*      pool = {};
        VkCommandBuffer buffer = {};

    public:
        void Clear(Image* image, Vec4 color);
        void Transition(Image* image, VkImageLayout newLayout);
        void TransitionMip(Image* image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mip);
    };

// -----------------------------------------------------------------------------

    struct Context
    {
        VkInstance  instance = {};
        VkPhysicalDevice gpu = {};
        VkDevice      device = {};
        VmaAllocator     vma = {};

        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorSizes = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
        };

        Fence* transferFence = {};

        Queue* graphics = {};

        Buffer*                  staging = {};
        CommandPool* transferCommandPool = {};
        CommandList*         transferCmd = {};

        static std::atomic_int64_t AllocationCount;
        static std::atomic_int64_t NewAllocationCount;

        VkAllocationCallbacks alloc = {
            .pfnAllocation = +[](void*, size_t size, size_t align, [[maybe_unused]] VkSystemAllocationScope scope) {
                void* ptr = _aligned_malloc(size, align);
                if (ptr)
                {
                    ++AllocationCount;
                    ++NewAllocationCount;
                }
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                if (ptr)
                {
                    std::cout << '\n' << std::stacktrace::current() << '\n';
                    NOVA_LOG("Allocating size = {}, align = {}, scope = {}, ptr = {}", size, align, int(scope), ptr);
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
                if (ptr)
                    --AllocationCount;
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                if (ptr)
                {
                    NOVA_LOG("Freeing ptr = {}", ptr);
                    NOVA_LOG("    Allocations - :: {}", AllocationCount.load());
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
        static Context* Create(bool debug);
        static void Destroy(Context* context);

        Buffer* CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, BufferFlags flags = {});
        void DestroyBuffer(Buffer* buffer);
        void CopyToBuffer(Buffer* buffer, const void* data, size_t size, VkDeviceSize offset = 0);

        Image* CreateImage(Vec3U size, VkImageUsageFlags usage, VkFormat format, ImageFlags flags = {});
        void DestroyImage(Image* image);
        void CopyToImage(Image* image, const void* data, size_t size);
        void GenerateMips(Image* image);
        // void Transition(VkCommandBuffer cmd, Image* image, VkImageLayout newLayout);
        // void TransitionMip(VkCommandBuffer cmd, Image* image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mip);

        Shader* CreateShader(VkShaderStageFlagBits stage, VkShaderStageFlags nextStage,
            const std::string& filename, const std::string& sourceCode = {},
            std::initializer_list<VkPushConstantRange> pushConstantRanges = {},
            std::initializer_list<VkDescriptorSetLayout> descriptorSetLayouts = {});
        void DestroyShader(Shader* shader);

        Swapchain* CreateSwapchain(VkSurfaceKHR surface, VkImageUsageFlags usage, VkPresentModeKHR presentMode);
        void Destroy(Swapchain* swapchain);

        VkSurfaceKHR CreateSurface(HWND hwnd);
        void Destroy(VkSurfaceKHR surface);

        CommandPool* CreateCommands();
        void Destroy(CommandPool* commands);

        Fence* CreateFence();
        void Destroy(Fence* fence);

        void WaitForIdle();

        template<class ...Objects>
        void Destroy(Objects*... objects)
        {
            (Destroy(objects), ...);
        }
    };
}
