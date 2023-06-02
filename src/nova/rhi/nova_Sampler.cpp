#include "nova_RHI.hpp"

namespace nova
{
    Sampler::Sampler(Context& _context, Filter filter, AddressMode addressMode, BorderColor color, f32 anistropy)
        : context(&_context)
    {
        VkCall(vkCreateSampler(context->device, Temp(VkSamplerCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VkFilter(filter),
            .minFilter = VkFilter(filter),
            // TODO: Separate option
            .mipmapMode = filter == Filter::Linear
                ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                : VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VkSamplerAddressMode(addressMode),
            .addressModeV = VkSamplerAddressMode(addressMode),
            .addressModeW = VkSamplerAddressMode(addressMode),
            .anisotropyEnable = anistropy > 0.f,
            .maxAnisotropy = anistropy,
            .minLod = -1000.f,
            .maxLod = 1000.f,
            .borderColor = VkBorderColor(color),
        }), context->pAlloc, &sampler));
    }

    Sampler::~Sampler()
    {
        if (sampler)
            vkDestroySampler(context->device, sampler, context->pAlloc);
    }

    Sampler::Sampler(Sampler&& other) noexcept
        : context(other.context)
        , sampler(other.sampler)
    {
        other.sampler = nullptr;
    }
}