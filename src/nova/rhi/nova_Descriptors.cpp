#include "nova_RHI.hpp"

namespace nova
{
    NOVA_NO_INLINE
    DescriptorLayout* Context::CreateDescriptorLayout(Span<DescriptorBinding> bindings, bool pushDescriptor)
    {
        auto* layout = new DescriptorLayout;
        layout->context = this;
        NOVA_ON_SCOPE_FAILURE(&) { DestroyDescriptorLayout(layout); };

        auto flags = NOVA_ALLOC_STACK(VkDescriptorBindingFlags, bindings.size());
        auto vkBindings = NOVA_ALLOC_STACK(VkDescriptorSetLayoutBinding, bindings.size());

        for (u32 i = 0; i < bindings.size(); ++i)
        {
            auto& binding = bindings.begin()[i];

            vkBindings[i] = {
                .binding = i,
                .descriptorType = VkDescriptorType(binding.type),
                .descriptorCount = binding.count,
                .stageFlags = VK_SHADER_STAGE_ALL,
            };

            // TODO: Partially bound optional?
            flags[i] = VkDescriptorBindingFlags(0);
            if (binding.count > 1)
                flags[i] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        }

        VkCall(vkCreateDescriptorSetLayout(device, Temp(VkDescriptorSetLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = Temp(VkDescriptorSetLayoutBindingFlagsCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = u32(bindings.size()),
                .pBindingFlags = flags,
            }),
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                | (pushDescriptor
                    ? VkDescriptorBindingFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR)
                    : VkDescriptorBindingFlags(0)),
            .bindingCount = u32(bindings.size()),
            .pBindings = vkBindings,
        }), pAlloc, &layout->layout));

        vkGetDescriptorSetLayoutSizeEXT(device, layout->layout, &layout->size);

        layout->offsets.resize(bindings.size());
        for (u32 i = 0; i < bindings.size(); ++i)
            vkGetDescriptorSetLayoutBindingOffsetEXT(device, layout->layout, i, &layout->offsets[i]);

        return layout;
    }

    void Context::DestroyDescriptorLayout(DescriptorLayout* layout)
    {
        vkDestroyDescriptorSetLayout(device, layout->layout, pAlloc);
        delete layout;
    }

    void DescriptorLayout::WriteSampledTexture(b8* dst, u32 binding, Texture* texture, Sampler* sampler, u32 arrayIndex)
    {
        vkGetDescriptorEXT(context->device,
            Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .data = {
                    .pCombinedImageSampler = Temp(VkDescriptorImageInfo {
                        .sampler = sampler->sampler,
                        .imageView = texture->view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    }),
                },
            }),
            context->descriptorSizes.combinedImageSamplerDescriptorSize,
            dst + offsets[binding]
                + (arrayIndex * context->descriptorSizes.combinedImageSamplerDescriptorSize));
    }

// -----------------------------------------------------------------------------

    void CommandList::PushStorageTexture(PipelineLayout* layout, u32 setIndex, u32 binding, Texture* texture, u32 arrayIndex)
    {
        vkCmdPushDescriptorSetKHR(buffer,
            layout->bindPoint, layout->layout, setIndex,
            1, Temp(VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = binding,
                .dstArrayElement = arrayIndex,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = Temp(VkDescriptorImageInfo {
                    .imageView = texture->view,
                    .imageLayout = tracker->Get(texture).layout,
                }),
            }));
    }

    void CommandList::PushAccelerationStructure(PipelineLayout* layout, u32 setIndex, u32 binding, AccelerationStructure* accelerationStructure, u32 arrayIndex)
    {
        vkCmdPushDescriptorSetKHR(buffer,
            layout->bindPoint, layout->layout, setIndex,
            1, Temp(VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = Temp(VkWriteDescriptorSetAccelerationStructureKHR {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                    .accelerationStructureCount = 1,
                    .pAccelerationStructures = &accelerationStructure->structure,
                }),
                .dstBinding = binding,
                .dstArrayElement = arrayIndex,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            }));
    }

// -----------------------------------------------------------------------------

    PipelineLayout* Context::CreatePipelineLayout(
            Span<PushConstantRange> pushConstantRanges,
            Span<DescriptorLayout*> descriptorLayouts,
            VkPipelineBindPoint bindPoint)
    {
        auto* layout = new PipelineLayout;
        layout->context = this;
        NOVA_ON_SCOPE_FAILURE(&) { DestroyPipelineLayout(layout); };

        for (auto& range : pushConstantRanges)
            layout->ranges.emplace_back(VkShaderStageFlags(range.stages), range.offset, range.size);

        for (auto& setLayout : descriptorLayouts)
            layout->sets.emplace_back(setLayout->layout);

        VkCall(vkCreatePipelineLayout(device, Temp(VkPipelineLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = u32(layout->sets.size()),
            .pSetLayouts = layout->sets.data(),
            .pushConstantRangeCount = u32(layout->ranges.size()),
            .pPushConstantRanges = layout->ranges.data(),
        }), pAlloc, &layout->layout));

        layout->bindPoint = bindPoint;

        return layout;
    }

    void Context::DestroyPipelineLayout(PipelineLayout* layout)
    {
        vkDestroyPipelineLayout(device, layout->layout, pAlloc);

        delete layout;
    }

// -----------------------------------------------------------------------------

    NOVA_NO_INLINE
    void CommandList::BindDescriptorBuffers(Span<Buffer*> buffers)
    {
        auto* bindings = NOVA_ALLOC_STACK(VkDescriptorBufferBindingInfoEXT, buffers.size());
        for (u32 i = 0; i < buffers.size(); ++i)
        {
            auto& descBuffer = buffers.begin()[i];

            bindings[i] = VkDescriptorBufferBindingInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                .address = descBuffer->address,
                .usage = descBuffer->usage,
            };
        }

        vkCmdBindDescriptorBuffersEXT(buffer, u32(buffers.size()), bindings);
    }

    NOVA_NO_INLINE
    void CommandList::SetDescriptorSetOffsets(PipelineLayout* layout, u32 firstSet, Span<DescriptorSetBindingOffset> offsets)
    {
        auto* bufferIndices = NOVA_ALLOC_STACK(u32, offsets.size());
        auto* bufferOffsets = NOVA_ALLOC_STACK(u64, offsets.size());

        for (u32 i = 0; i < offsets.size(); ++i)
        {
            auto& offset = offsets.begin()[i];

            bufferIndices[i] = offset.buffer;
            bufferOffsets[i] = offset.offset;
        }

        vkCmdSetDescriptorBufferOffsetsEXT(buffer,
            layout->bindPoint, layout->layout,
            firstSet, u32(offsets.size()),
            bufferIndices, bufferOffsets);
    }
}