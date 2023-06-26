#include "nova_VulkanRHI.hpp"

namespace nova
{
    DescriptorSetLayout VulkanContext::Descriptors_CreateSetLayout(Span<DescriptorBinding> bindings, bool pushDescriptors)
    {
        auto[id, setLayout] = descriptorSetLayouts.Acquire();

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

            flags[i] |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
            flags[i] |= VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

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
                },
                [&](const binding::UniformBuffer& binding) {
                    vkBindings[i].descriptorType = binding.dynamic
                        ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                        : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    vkBindings[i].descriptorCount = binding.count.value_or(1u);
                    if (binding.count)
                        flags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
                },
            }, binding);
        }

        VkCall(vkCreateDescriptorSetLayout(device, Temp(VkDescriptorSetLayoutCreateInfo {
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
        }), pAlloc, &setLayout.layout));

        setLayout.bindings.reserve(bindings.size());
        std::move(bindings.begin(), bindings.end(), std::back_insert_iterator(setLayout.bindings));

        return id;
    }

    void VulkanContext::Descriptors_DestroySetLayout(DescriptorSetLayout id)
    {
        if (Get(id).layout)
            vkDestroyDescriptorSetLayout(device, Get(id).layout, pAlloc);

        descriptorSetLayouts.Return(id);
    }

    DescriptorSet VulkanContext::Descriptors_AllocateSet(DescriptorSetLayout layout, u64 customSize)
    {
        (void)customSize; // TODO

        auto[id, set] = descriptorSets.Acquire();

        vkAllocateDescriptorSets(device, Temp(VkDescriptorSetAllocateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &Get(layout).layout,
        }), &set.set);

        return id;
    }

    void VulkanContext::Descriptors_FreeSet(DescriptorSet set)
    {
        if (Get(set).set)
            vkFreeDescriptorSets(device, descriptorPool, 1, &Get(set).set);

        descriptorSets.Return(set);
    }

// -----------------------------------------------------------------------------

    void VulkanContext::Descriptors_WriteSampledTexture(DescriptorSet set, u32 binding, Texture texture, Sampler sampler, u32 arrayIndex)
    {
        vkUpdateDescriptorSets(device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = Get(set).set,
            .dstBinding = binding,
            .dstArrayElement = arrayIndex,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = Temp(VkDescriptorImageInfo {
                .sampler = Get(sampler).sampler,
                .imageView = Get(texture).view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            }),
        }), 0, nullptr);
    }

    void VulkanContext::Descriptors_WriteUniformBuffer(DescriptorSet set, u32 binding, Buffer buffer, u32 arrayIndex)
    {
        vkUpdateDescriptorSets(device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = Get(set).set,
            .dstBinding = binding,
            .dstArrayElement = arrayIndex,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = Temp(VkDescriptorBufferInfo {
                .buffer = Get(buffer).buffer,
                .range = VK_WHOLE_SIZE,
            }),
        }), 0, nullptr);
    }

// -----------------------------------------------------------------------------

    void VulkanContext::Cmd_BindDescriptorSets(CommandList cmd, PipelineLayout pipelineLayout, u32 firstSet, Span<DescriptorSet> sets)
    {
        auto vkSets = NOVA_ALLOC_STACK(VkDescriptorSet, sets.size());
        for (u32 i = 0; i < sets.size(); ++i)
            vkSets[i] = Get(sets[i]).set;

        vkCmdBindDescriptorSets(Get(cmd).buffer, VkPipelineBindPoint(Get(pipelineLayout).bindPoint),
            Get(pipelineLayout).layout, firstSet,
            u32(sets.size()), vkSets,
            0, nullptr);
    }

    void VulkanContext::Cmd_PushStorageTexture(CommandList cmd, PipelineLayout layout, u32 setIndex, u32 binding, Texture texture, u32 arrayIndex)
    {
        vkCmdPushDescriptorSetKHR(Get(cmd).buffer,
            VkPipelineBindPoint(Get(layout).bindPoint), Get(layout).layout, setIndex,
            1, Temp(VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = binding,
                .dstArrayElement = arrayIndex,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = Temp(VkDescriptorImageInfo {
                    .imageView = Get(texture).view,
                    .imageLayout = Get(Get(cmd).state).imageStates[Get(texture).image].layout,
                }),
            }));
    }

    void VulkanContext::Cmd_PushAccelerationStructure(CommandList cmd, PipelineLayout layout, u32 setIndex, u32 binding, AccelerationStructure accelerationStructure, u32 arrayIndex)
    {
        vkCmdPushDescriptorSetKHR(Get(cmd).buffer,
            VkPipelineBindPoint(Get(layout).bindPoint), Get(layout).layout, setIndex,
            1, Temp(VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = Temp(VkWriteDescriptorSetAccelerationStructureKHR {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                    .accelerationStructureCount = 1,
                    .pAccelerationStructures = &Get(accelerationStructure).structure,
                }),
                .dstBinding = binding,
                .dstArrayElement = arrayIndex,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            }));
    }
}