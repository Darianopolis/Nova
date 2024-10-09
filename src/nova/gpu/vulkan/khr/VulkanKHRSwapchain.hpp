#include <nova/gpu/vulkan/VulkanRHI.hpp>

namespace nova
{
    struct KHRSwapchainData : Handle<Swapchain>::Impl
    {
        Context context = {};

        VkSurfaceKHR      surface = {};
        VkSwapchainKHR  swapchain = {};
        VkSurfaceFormatKHR format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        ImageUsage          usage = {};
        PresentMode  present_mode = PresentMode::Fifo;
        std::vector<Image> images = {};
        uint32_t            index = UINT32_MAX;
        VkExtent2D         extent = { 0, 0 };
        bool              invalid = false;

        std::vector<VkSemaphore> semaphores = {};
        u32                 semaphore_index = 0;

        static KHRSwapchainData* Get(Swapchain swapchain)
        {
            return static_cast<KHRSwapchainData*>(swapchain.operator->());
        }
    };

    Swapchain KHRSwapchain_Create(HContext context, Window window, ImageUsage usage, PresentMode present_mode);
}