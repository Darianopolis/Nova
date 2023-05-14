#pragma once

#include <Core/Core.hpp>
#include <Core/Ref.hpp>

namespace pyr
{
    inline
    void VkCall(VkResult res, std::source_location loc = std::source_location::current())
    {
        if (res != VK_SUCCESS)
            Error(std::format("Error: {} ({} @ {})\n", int(res), loc.line(), loc.file_name()));
    }

    #define PYR_VKQUERY(name, func, ...) do {                \
        u32 count;                                           \
        func(__VA_ARGS__ __VA_OPT__(,) &count, nullptr);     \
        name.resize(count);                                  \
        func(__VA_ARGS__ __VA_OPT__(,) &count, name.data()); \
    } while(0)

// -----------------------------------------------------------------------------

    #define PYR_DECL_DEVICE_PROC(name)  inline PFN_##name name
    #define PYR_LOAD_DEVICE_PROC(name, device) ::pyr::name = (PFN_##name)vkGetDeviceProcAddr(device, #name);\
        PYR_LOG("Loaded fn [" #name "] - {}", (void*)name)

// -----------------------------------------------------------------------------

    struct Image;
    struct Buffer;
    struct Shader;
    struct Context;
    struct Commands;

// -----------------------------------------------------------------------------

    enum class BufferFlags
    {
        None,
        Addressable  = 1 << 0,
        DeviceLocal  = 1 << 1 | Addressable,
        Mappable     = 1 << 2,
        CreateMapped = 1 << 3 | Mappable,
    };
    PYR_DECORATE_FLAG_ENUM(BufferFlags)

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
    PYR_DECORATE_FLAG_ENUM(BufferUsage)

    struct Buffer : RefCounted
    {
        Ref<Context> context = {};

        VkBuffer          buffer = {};
        VmaAllocation allocation = {};
        VkDeviceSize        size = 0ull;
        VkDeviceAddress  address = 0ull;
        byte*             mapped = nullptr;
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
    PYR_DECORATE_FLAG_ENUM(ImageFlags)

    enum class ImageUsage
    {
        Sampled    = 1 << 0,
        Attachment = 1 << 1,
        Storage    = 1 << 2,
    };
    PYR_DECORATE_FLAG_ENUM(ImageUsage)

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
        Ref<Context> context = {};

        VkImage             image = {};
        VmaAllocation  allocation = {};
        VkImageView          view = {};
        VkImageLayout      layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkFormat           format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

        uvec3 extent = {};
        u32     mips = 0;
        u32   layers = 0;

    public:
        ~Image();
    };


// -----------------------------------------------------------------------------

    struct Shader : RefCounted
    {
        Ref<Context> context = {};

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
        Ref<Context>      context = {};

        VkSurfaceKHR           surface = nullptr;
        VkSwapchainKHR       swapchain = nullptr;
        VkSurfaceFormatKHR      format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        VkImageUsageFlags        usage = 0;
        VkPresentModeKHR   presentMode = VK_PRESENT_MODE_FIFO_KHR;
        std::vector<Ref<Image>> images = {};
        uint32_t                 index = UINT32_MAX;
        Image*                   image = nullptr;
        VkExtent2D              extent = { 0, 0 };

    public:
        ~Swapchain();
    };

// -----------------------------------------------------------------------------

    // struct Commands
    // {
    //     VkCommandPool                   pool = {};
    //     std::vector<VkCommandBuffer> buffers = {};
    //     u32                            index = 0;
    // };

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

        VkQueue queue;
        u32 queueFamily;
        VkCommandBuffer cmd;
        VkCommandPool pool;
        VkCommandBuffer transferCmd;
        VkCommandPool transferPool;

        VkFence fence;

        Ref<Buffer> staging;
    public:
        ~Context();

        Ref<Buffer> CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, BufferFlags flags = {});
        // void DestroyBuffer(Buffer& buffer);
        void CopyToBuffer(Buffer& buffer, const void* data, size_t size, VkDeviceSize offset = 0);

        Ref<Image> CreateImage(uvec3 size, VkImageUsageFlags usage, VkFormat format, ImageFlags flags = {});
        // void DestroyImage(Image& image);
        void CopyToImage(Image& image, const void* data, size_t size);
        void GenerateMips(Image& image);
        void Transition(VkCommandBuffer cmd, Image& image, VkImageLayout newLayout);
        void TransitionMip(VkCommandBuffer cmd, Image& image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mip);

        Ref<Shader> CreateShader(VkShaderStageFlagBits stage, VkShaderStageFlags nextStage,
            const std::string& filename, const std::string& sourceCode = {},
            std::initializer_list<VkPushConstantRange> pushConstantRanges = {},
            std::initializer_list<VkDescriptorSetLayout> descriptorSetLayouts = {});
        // void DestroyShader(Shader& shader);

        Ref<Swapchain> CreateSwapchain(VkSurfaceKHR surface, VkImageUsageFlags usage, VkPresentModeKHR presentMode);
        void Present(Swapchain& swapchain);
        bool GetNextImage(Swapchain& swapchain);
        // void DestroySwapchain(Swapchain& swapchain);

        // Commands CreateCommands(Context* ctx, Queue* queue);
        // VkCommandBuffer AllocCmdBuf(Context* ctx, Commands* cmds);
        // void FlushCommands(Context* ctx, Commands* cmds, Queue* queue);
        void Flush(VkCommandBuffer commandBuffer = {});
    };

    Ref<Context> CreateContext(b8 debug);
}