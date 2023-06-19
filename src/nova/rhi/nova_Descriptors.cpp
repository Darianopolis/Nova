#include "nova_RHI_Impl.hpp"

namespace nova
{
    NOVA_DEFINE_HANDLE_OPERATIONS(DescriptorSetLayout)

    NOVA_NO_INLINE
    DescriptorSetLayout::DescriptorSetLayout(Context context, Span<DescriptorBinding> bindings, bool pushDescriptor)
        : ImplHandle(new DescriptorSetLayoutImpl)
    {
        impl->context = context;

        auto flags = NOVA_ALLOC_STACK(VkDescriptorBindingFlags, bindings.size());
        auto vkBindings = NOVA_ALLOC_STACK(VkDescriptorSetLayoutBinding, bindings.size());

        for (u32 i = 0; i < bindings.size(); ++i)
        {
            auto& binding = bindings[i];

            vkBindings[i] = {
                .binding = i,
                .stageFlags = VK_SHADER_STAGE_ALL,
            };

            // TODO: Partially bound optional?
            flags[i] = VkDescriptorBindingFlags(0);

            std::visit(Overloads {
                [&](const binding::SampledTexture& binding) {
                    vkBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    vkBindings[i].descriptorCount = binding.count.value_or(1u);
                    if (binding.count)
                        flags[i] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
                },
                [&](const binding::StorageTexture& binding) {
                    vkBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    vkBindings[i].descriptorCount = binding.count.value_or(1u);
                    if (binding.count)
                        flags[i] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
                },
                [&](const binding::AccelerationStructure& binding) {
                    vkBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                    vkBindings[i].descriptorCount = binding.count.value_or(1u);
                    if (binding.count)
                        flags[i] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
                }
            }, binding);
        }

        VkCall(vkCreateDescriptorSetLayout(context->device, Temp(VkDescriptorSetLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = Temp(VkDescriptorSetLayoutBindingFlagsCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = u32(bindings.size()),
                .pBindingFlags = flags,
            }),
            .flags = (context->config.descriptorBuffers
                    ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                    : VkDescriptorBindingFlags(0))
                | (pushDescriptor
                    ? VkDescriptorBindingFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR)
                    : VkDescriptorBindingFlags(0)),
            .bindingCount = u32(bindings.size()),
            .pBindings = vkBindings,
        }), context->pAlloc, &impl->layout));

        if (context->config.descriptorBuffers)
        {
            vkGetDescriptorSetLayoutSizeEXT(context->device, impl->layout, &impl->size);

            impl->offsets.resize(bindings.size());
            for (u32 i = 0; i < bindings.size(); ++i)
                vkGetDescriptorSetLayoutBindingOffsetEXT(context->device, impl->layout, i, &impl->offsets[i]);
        }

        impl->bindings.reserve(bindings.size());
        std::move(bindings.begin(), bindings.end(), std::back_insert_iterator(impl->bindings));
    }

    DescriptorSetLayoutImpl::~DescriptorSetLayoutImpl()
    {
        if (layout)
            vkDestroyDescriptorSetLayout(context->device, layout, context->pAlloc);
    }

    u64 DescriptorSetLayout::GetSize() const noexcept
    {
        return impl->size;
    }

    void DescriptorSetLayout::WriteSampledTexture(void* dst, u32 binding, Texture texture, Sampler sampler, u32 arrayIndex) const noexcept
    {
        vkGetDescriptorEXT(impl->context->device,
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
            impl->context->descriptorSizes.combinedImageSamplerDescriptorSize,
            static_cast<b8*>(dst) + impl->offsets[binding]
                + (arrayIndex * impl->context->descriptorSizes.combinedImageSamplerDescriptorSize));
    }

    void DescriptorSet::WriteSampledTexture(u32 binding, Texture texture, Sampler sampler, u32 arrayIndex) const noexcept
    {
        auto& context = impl->layout->context;

        vkUpdateDescriptorSets(context->device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = impl->set,
            .dstBinding = binding,
            .dstArrayElement = arrayIndex,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = Temp(VkDescriptorImageInfo {
                .sampler = sampler->sampler,
                .imageView = texture->view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            }),
        }), 0, nullptr);
    }

// -----------------------------------------------------------------------------

    void CommandList::PushStorageTexture(PipelineLayout layout, u32 setIndex, u32 binding, Texture texture, u32 arrayIndex) const
    {
        vkCmdPushDescriptorSetKHR(impl->buffer,
            VkPipelineBindPoint(layout->bindPoint), layout->layout, setIndex,
            1, Temp(VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = binding,
                .dstArrayElement = arrayIndex,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = Temp(VkDescriptorImageInfo {
                    .imageView = texture->view,
                    .imageLayout = impl->state->Get(texture).layout,
                }),
            }));
    }

    void CommandList::PushAccelerationStructure(PipelineLayout layout, u32 setIndex, u32 binding, AccelerationStructure accelerationStructure, u32 arrayIndex) const
    {
        vkCmdPushDescriptorSetKHR(impl->buffer,
            VkPipelineBindPoint(layout->bindPoint), layout->layout, setIndex,
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

    NOVA_DEFINE_HANDLE_OPERATIONS(DescriptorSet);

    DescriptorSet::DescriptorSet(DescriptorSetLayout layout, u64 customSize)
        : ImplHandle(new DescriptorSetImpl)
    {
        (void)customSize;

        impl->layout = layout;

        std::scoped_lock lock { layout->context->descriptorPoolMutex };
        vkAllocateDescriptorSets(layout->context->device, Temp(VkDescriptorSetAllocateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = layout->context->descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout->layout,
        }), &impl->set);
    }

    DescriptorSetImpl::~DescriptorSetImpl()
    {
        if (set)
        {
            std::scoped_lock lock { layout->context->descriptorPoolMutex };
            vkFreeDescriptorSets(layout->context->device, layout->context->descriptorPool, 1, &set);
        }
    }

    void CommandList::BindDescriptorSets(PipelineLayout pipelineLayout, u32 firstSet, Span<DescriptorSet> sets) const
    {
        auto vkSets = NOVA_ALLOC_STACK(VkDescriptorSet, sets.size());
        for (u32 i = 0; i < sets.size(); ++i)
            vkSets[i] = sets[i]->set;

        vkCmdBindDescriptorSets(impl->buffer, VkPipelineBindPoint(pipelineLayout->bindPoint),
            pipelineLayout->layout, firstSet,
            u32(sets.size()), vkSets,
            0, nullptr);
    }

// -----------------------------------------------------------------------------

    NOVA_DEFINE_HANDLE_OPERATIONS(PipelineLayout)

    PipelineLayout::PipelineLayout(Context context,
            Span<PushConstantRange> pushConstantRanges,
            Span<DescriptorSetLayout> descriptorLayouts,
            BindPoint bindPoint)
        : ImplHandle(new PipelineLayoutImpl)
    {
        impl->context = context;
        impl->bindPoint = bindPoint;
        impl->id = context->GetUID();

        for (auto& range : pushConstantRanges)
            impl->ranges.emplace_back(VkShaderStageFlags(range.stages), range.offset, range.size);

        for (auto& setLayout : descriptorLayouts)
            impl->sets.emplace_back(setLayout->layout);

        VkCall(vkCreatePipelineLayout(context->device, Temp(VkPipelineLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = u32(impl->sets.size()),
            .pSetLayouts = impl->sets.data(),
            .pushConstantRangeCount = u32(impl->ranges.size()),
            .pPushConstantRanges = impl->ranges.data(),
        }), context->pAlloc, &impl->layout));
    }

    PipelineLayoutImpl::~PipelineLayoutImpl()
    {
        context->PushDeletedObject(id);

        if (layout)
            vkDestroyPipelineLayout(context->device, layout, context->pAlloc);
    }

// -----------------------------------------------------------------------------

    NOVA_NO_INLINE
    void CommandList::BindDescriptorBuffers(Span<Buffer> buffers) const
    {
        auto bindings = NOVA_ALLOC_STACK(VkDescriptorBufferBindingInfoEXT, buffers.size());
        for (u32 i = 0; i < buffers.size(); ++i)
        {
            auto& descBuffer = buffers[i];

            bindings[i] = VkDescriptorBufferBindingInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                .address = descBuffer->address,
                .usage = descBuffer->usage,
            };
        }

        vkCmdBindDescriptorBuffersEXT(impl->buffer, u32(buffers.size()), bindings);
    }

    NOVA_NO_INLINE
    void CommandList::SetDescriptorSetOffsets(PipelineLayout layout, u32 firstSet, Span<DescriptorSetBindingOffset> offsets) const
    {
        auto bufferIndices = NOVA_ALLOC_STACK(u32, offsets.size());
        auto bufferOffsets = NOVA_ALLOC_STACK(u64, offsets.size());

        for (u32 i = 0; i < offsets.size(); ++i)
        {
            auto& offset = offsets[i];

            bufferIndices[i] = offset.buffer;
            bufferOffsets[i] = offset.offset;
        }

        vkCmdSetDescriptorBufferOffsetsEXT(impl->buffer,
            VkPipelineBindPoint(layout->bindPoint), layout->layout,
            firstSet, u32(offsets.size()),
            bufferIndices, bufferOffsets);
    }
}