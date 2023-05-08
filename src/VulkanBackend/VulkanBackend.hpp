#pragma once

#include <Core/Core.hpp>

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

    struct Buffer
    {
        VkBuffer          buffer = {};
        VmaAllocation allocation = {};
        VkDeviceSize        size = 0ull;
        VkDeviceAddress  address = 0ull;
        byte*             mapped = nullptr;
        BufferFlags        flags = BufferFlags::None;
    };


// -----------------------------------------------------------------------------

    enum class ImageFlags
    {
        Array = 1 << 0,
        Mips  = 1 << 1,
    };
    PYR_DECORATE_FLAG_ENUM(ImageFlags)

    struct Image
    {
        VkImage             image = {};
        VmaAllocation  allocation = {};
        VkImageView          view = {};
        VkImageLayout      layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkFormat           format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

        uvec3 extent = {};
        u32     mips = 0;
        u32   layers = 0;
    };


// -----------------------------------------------------------------------------

    struct Shader
    {
        VkShaderStageFlagBits stage;
        VkShaderEXT          shader;
        VkShaderModule       module;

        VkPipelineShaderStageCreateInfo stageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .module = {},
            .pName = "main",
        };
    };

// -----------------------------------------------------------------------------

    struct Swapchain
    {
        VkSurfaceKHR         surface = nullptr;
        VkSwapchainKHR     swapchain = nullptr;
        VkSurfaceFormatKHR    format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        VkImageUsageFlags      usage = 0;
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        std::vector<Image>    images = {};
        uint32_t               index = UINT32_MAX;
        Image*                 image = nullptr;
        VkExtent2D            extent = { 0, 0 };
    };

// -----------------------------------------------------------------------------

    // struct Commands
    // {
    //     VkCommandPool                   pool = {};
    //     std::vector<VkCommandBuffer> buffers = {};
    //     u32                            index = 0;
    // };

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

        VkQueue queue;
        u32 queueFamily;
        VkCommandBuffer cmd;
        VkCommandPool pool;
        VkCommandBuffer transferCmd;
        VkCommandPool transferPool;

        VkFence fence;

        Buffer staging;
    public:
        Buffer CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, BufferFlags flags = {});
        void DestroyBuffer(Buffer& buffer);
        void CopyToBuffer(Buffer& buffer, const void* data, size_t size, VkDeviceSize offset = 0);

        Image CreateImage(uvec3 size, VkImageUsageFlags usage, VkFormat format, ImageFlags flags = {});
        void DestroyImage(Image& image);
        void CopyToImage(Image& image, const void* data, size_t size);
        void GenerateMips(Image& image);
        void Transition(VkCommandBuffer cmd, Image& image, VkImageLayout newLayout);
        void TransitionMip(VkCommandBuffer cmd, Image& image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mip);

        Shader CreateShader(VkShaderStageFlagBits stage, VkShaderStageFlags nextStage,
            const std::string& filename, const std::string& sourceCode = {},
            std::initializer_list<VkPushConstantRange> pushConstantRanges = {},
            std::initializer_list<VkDescriptorSetLayout> descriptorSetLayouts = {});
        void DestroyShader(Shader& shader);

        Swapchain CreateSwapchain(VkSurfaceKHR surface, VkImageUsageFlags usage, VkPresentModeKHR presentMode);
        void Present(Swapchain& swapchain);
        bool GetNextImage(Swapchain& swapchain);
        void DestroySwapchain(Swapchain& swapchain);

        // Commands CreateCommands(Context* ctx, Queue* queue);
        // VkCommandBuffer AllocCmdBuf(Context* ctx, Commands* cmds);
        // void FlushCommands(Context* ctx, Commands* cmds, Queue* queue);
        void Flush(VkCommandBuffer commandBuffer = {});
    };

    Context CreateContext(b8 debug);
}