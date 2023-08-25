#include "nova_VulkanRHI.hpp"

namespace nova
{
    DescriptorSetLayout DescriptorSetLayout::Create(HContext context, Span<DescriptorBinding> bindings, bool pushDescriptors)
    {
        auto impl = new Impl;
        impl->context = context;

        auto flags = NOVA_ALLOC_STACK(VkDescriptorBindingFlags, bindings.size());
        auto vkBindings = NOVA_ALLOC_STACK(VkDescriptorSetLayoutBinding, bindings.size());

        for (u32 i = 0; i < bindings.size(); ++i) {
            auto& binding = bindings[i];

            vkBindings[i] = {
                .binding = i,
                .stageFlags = VK_SHADER_STAGE_ALL,
            };

            // TODO: Partially bound optional?
            flags[i] = VkDescriptorBindingFlags(0);

            if (!pushDescriptors) {
                flags[i] |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
                flags[i] |= VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
            }

            std::visit(Overloads {
                [&](const binding::SampledTexture& binding) {
                    vkBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    vkBindings[i].descriptorCount = binding.count.value_or(1u);
                    if (binding.count) {
                        flags[i] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
                    }
                },
                [&](const binding::StorageTexture& binding) {
                    vkBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    vkBindings[i].descriptorCount = binding.count.value_or(1u);
                    if (binding.count) {
                        flags[i] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
                    }
                },
                [&](const binding::AccelerationStructure& binding) {
                    vkBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                    vkBindings[i].descriptorCount = binding.count.value_or(1u);
                    if (binding.count) {
                        flags[i] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
                    }
                },
                [&](const binding::UniformBuffer& binding) {
                    vkBindings[i].descriptorType = binding.dynamic
                        ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                        : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    vkBindings[i].descriptorCount = binding.count.value_or(1u);
                    if (binding.count) {
                        flags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
                    }
                },
            }, binding);
        }

        VkCall(vkCreateDescriptorSetLayout(context->device, Temp(VkDescriptorSetLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = Temp(VkDescriptorSetLayoutBindingFlagsCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = u32(bindings.size()),
                .pBindingFlags = flags,
            }),
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT
                | (pushDescriptors
                    ? VkDescriptorBindingFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR)
                    : VkDescriptorBindingFlags(0)),
            .bindingCount = u32(bindings.size()),
            .pBindings = vkBindings,
        }), context->pAlloc, &impl->layout));

        impl->bindings.reserve(bindings.size());
        std::move(bindings.begin(), bindings.end(), std::back_insert_iterator(impl->bindings));

        return { impl };
    }

    void DescriptorSetLayout::Destroy()
    {
        if (!impl) {
            return;
        }
        
        vkDestroyDescriptorSetLayout(impl->context->device, impl->layout, impl->context->pAlloc);
        
        delete impl;
        impl = nullptr;
    }

    DescriptorSet DescriptorSet::Create(HDescriptorSetLayout layout, u64 customSize)
    {
        auto impl = new Impl;
        impl->layout = layout;

        (void)customSize; // TODO

        vkAllocateDescriptorSets(layout->context->device, Temp(VkDescriptorSetAllocateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = layout->context->descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout->layout,
        }), &impl->set);

        return { impl };
    }

    void DescriptorSet::Destroy()
    {
        if (!impl) {
            return;
        }
        
        vkFreeDescriptorSets(impl->layout->context->device, impl->layout->context->descriptorPool, 1, &impl->set);

        delete impl;
        impl = nullptr;
    }

// -----------------------------------------------------------------------------

    void DescriptorSet::WriteSampledTexture(u32 binding, HTexture texture, HSampler sampler, u32 arrayIndex) const
    {
        vkUpdateDescriptorSets(impl->layout->context->device, 1, Temp(VkWriteDescriptorSet {
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

    void DescriptorSet::WriteUniformBuffer(u32 binding, HBuffer buffer, u32 arrayIndex) const
    {
        vkUpdateDescriptorSets(impl->layout->context->device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = impl->set,
            .dstBinding = binding,
            .dstArrayElement = arrayIndex,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = Temp(VkDescriptorBufferInfo {
                .buffer = buffer->buffer,
                .range = VK_WHOLE_SIZE,
            }),
        }), 0, nullptr);
    }

// -----------------------------------------------------------------------------

    void CommandList::BindDescriptorSets(HPipelineLayout pipelineLayout, u32 firstSet, Span<HDescriptorSet> sets) const
    {
        auto vkSets = NOVA_ALLOC_STACK(VkDescriptorSet, sets.size());
        for (u32 i = 0; i < sets.size(); ++i) {
            vkSets[i] = sets[i]->set;
        }

        vkCmdBindDescriptorSets(impl->buffer, GetVulkanPipelineBindPoint(pipelineLayout->bindPoint),
            pipelineLayout->layout, firstSet,
            u32(sets.size()), vkSets,
            0, nullptr);
    }

    void CommandList::PushStorageTexture(HPipelineLayout layout, u32 setIndex, u32 binding, HTexture texture, u32 arrayIndex) const
    {
        vkCmdPushDescriptorSetKHR(impl->buffer,
            GetVulkanPipelineBindPoint(layout->bindPoint), layout->layout, setIndex,
            1, Temp(VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = binding,
                .dstArrayElement = arrayIndex,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = Temp(VkDescriptorImageInfo {
                    .imageView = texture->view,
                    .imageLayout = impl->state->imageStates[texture->image].layout,
                }),
            }));
    }

    void CommandList::PushAccelerationStructure(HPipelineLayout layout, u32 setIndex, u32 binding, HAccelerationStructure accelerationStructure, u32 arrayIndex) const
    {
        vkCmdPushDescriptorSetKHR(impl->buffer,
            GetVulkanPipelineBindPoint(layout->bindPoint), layout->layout, setIndex,
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
}