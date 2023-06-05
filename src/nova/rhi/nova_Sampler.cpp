#include "nova_RHI_Impl.hpp"

namespace nova
{
    NOVA_DEFINE_IMPL_HANDLE_OPERATIONS(Sampler)

    Sampler::Sampler(Context context, Filter filter, AddressMode addressMode, BorderColor color, f32 anistropy)
        : ImplHandle(new SamplerImpl)
    {
        impl->context = context.GetImpl();

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
        }), impl->context->pAlloc, &impl->sampler));
    }

    SamplerImpl::~SamplerImpl()
    {
        if (sampler)
            vkDestroySampler(context->device, sampler, context->pAlloc);
    }
}