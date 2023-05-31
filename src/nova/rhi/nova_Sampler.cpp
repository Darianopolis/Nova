#include "nova_RHI.hpp"

namespace nova
{
    Rc<Sampler> Context::CreateSampler(Filter filter, AddressMode addressMode, BorderColor color, f32 anistropy)
    {
        Rc sampler = new Sampler;
        sampler->context = this;

        VkCall(vkCreateSampler(device, Temp(VkSamplerCreateInfo {
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
        }), pAlloc, &sampler->sampler));

        return sampler;
    }

    Sampler::~Sampler()
    {
        vkDestroySampler(context->device, sampler, context->pAlloc);
    }
}