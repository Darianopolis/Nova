#include "nova_VulkanRHI.hpp"

namespace nova
{
    DescriptorHeap DescriptorHeap::Create(HContext context, u32 requestedDescriptorCount)
    {
        auto impl = new Impl;
        impl->context = context;

        impl->descriptorCount = std::min(requestedDescriptorCount, context->maxDescriptors);

        VkCall(vkCreateDescriptorPool(context->device, Temp(VkDescriptorPoolCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = Temp(VkMutableDescriptorTypeCreateInfoEXT {
                .sType = VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_VALVE,
                .mutableDescriptorTypeListCount = 1,
                .pMutableDescriptorTypeLists = Temp(VkMutableDescriptorTypeListEXT {
                    .descriptorTypeCount = 7,
                    .pDescriptorTypes = std::array {
                        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                        VK_DESCRIPTOR_TYPE_SAMPLER,
                    }.data(),
                }),
            }),
            .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets = u32(impl->descriptorCount),
            .poolSizeCount = 1,
            .pPoolSizes = std::array {
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLER, impl->descriptorCount },
            }.data(),
        }), context->pAlloc, &impl->descriptorPool));

        VkCall(vkAllocateDescriptorSets(context->device, Temp(VkDescriptorSetAllocateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = Temp(VkDescriptorSetVariableDescriptorCountAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
                .descriptorSetCount = 1,
                .pDescriptorCounts = Temp(u32(impl->descriptorCount)),
            }),
            .descriptorPool = impl->descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &context->heapLayout,
        }), &impl->descriptorSet));

        return{ impl };
    }

    void DescriptorHeap::Destroy()
    {
        if (!impl) {
            return;
        }

        vkDestroyDescriptorPool(impl->context->device, impl->descriptorPool, impl->context->pAlloc);

        delete impl;
        impl = nullptr;
    }

    u32 DescriptorHeap::GetMaxDescriptorCount() const
    {
        return impl->descriptorCount;
    }

// -----------------------------------------------------------------------------

    void CommandList::BindDescriptorHeap(BindPoint bindPoint, HDescriptorHeap heap) const
    {
        vkCmdBindDescriptorSets(impl->buffer, GetVulkanPipelineBindPoint(bindPoint),
            impl->pool->context->pipelineLayout, 0,
            1, std::array {
                heap->descriptorSet,
            }.data(),
            0, nullptr);
    }

// -----------------------------------------------------------------------------

    DescriptorHandle DescriptorHeap::WriteStorageBuffer(DescriptorHandle handle, HBuffer buffer, u64 size, u64 offset) const
    {
        vkUpdateDescriptorSets(impl->context->device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = impl->descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = handle.id,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = Temp(VkDescriptorBufferInfo {
                .buffer = buffer->buffer,
                .offset = offset,
                .range = size,
            }),
        }), 0, nullptr);

        return handle;
    }

    DescriptorHandle DescriptorHeap::WriteUniformBuffer(DescriptorHandle handle, HBuffer buffer, u64 size, u64 offset) const
    {
        vkUpdateDescriptorSets(impl->context->device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = impl->descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = handle.id,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = Temp(VkDescriptorBufferInfo {
                .buffer = buffer->buffer,
                .offset = offset,
                .range = size,
            }),
        }), 0, nullptr);

        return handle;
    }

    DescriptorHandle DescriptorHeap::WriteSampledTexture(DescriptorHandle handle, HTexture texture) const
    {
        vkUpdateDescriptorSets(impl->context->device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = impl->descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = handle.id,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = Temp(VkDescriptorImageInfo {
                .imageView = texture->view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            }),
        }), 0, nullptr);

        return handle;
    }

    DescriptorHandle DescriptorHeap::WriteStorageTexture(DescriptorHandle handle, HTexture texture) const
    {
        vkUpdateDescriptorSets(impl->context->device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = impl->descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = handle.id,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = Temp(VkDescriptorImageInfo {
                .imageView = texture->view,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            }),
        }), 0, nullptr);

        return handle;
    }

    DescriptorHandle DescriptorHeap::WriteSampler(DescriptorHandle handle, HSampler sampler) const
    {
        vkUpdateDescriptorSets(impl->context->device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = impl->descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = handle.id,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = Temp(VkDescriptorImageInfo {
                .sampler = sampler->sampler,
            }),
        }), 0, nullptr);

        return handle;
    }

    void CommandList::BindAccelerationStructure(BindPoint bindPoint, HAccelerationStructure accelerationStructure) const
    {
        vkCmdPushDescriptorSetKHR(impl->buffer,
            GetVulkanPipelineBindPoint(bindPoint),
            impl->pool->context->pipelineLayout, 1, 1, Temp(VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = Temp(VkWriteDescriptorSetAccelerationStructureKHR {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                    .accelerationStructureCount = 1,
                    .pAccelerationStructures = &accelerationStructure->structure,
                }),
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            }));
    }
}