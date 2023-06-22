#include "nova_VulkanContext.hpp"

namespace nova
{
    Texture VulkanContext::Texture_Create(Vec3U size, TextureUsage usage, Format format, TextureFlags flags)
    {
        (void)size;
        (void)usage;
        (void)format;
        (void)flags;

        return {};
    }

    void VulkanContext::Destroy(Texture id)
    {
        auto& texture = Get(id);
        if (texture.view)
            vkDestroyImageView(device, texture.view, pAlloc);

        if (texture.allocation)
            vmaDestroyImage(vma, texture.image, texture.allocation);

        textures.Return(id);
    }

    Vec3U VulkanContext::Texture_GetExtent(Texture id)
    {
        return Get(id).extent;
    }

    Format VulkanContext::Texture_GetFormat(Texture id)
    {
        return Format(Get(id).format);
    }
}