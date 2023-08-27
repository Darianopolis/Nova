#include "nova_VulkanRHI.hpp"

namespace nova
{
    DescriptorHeap DescriptorHeap::Create(HContext context)
    {
        auto impl = new Impl;
        impl->context = context;

        VkCall(vkAllocateDescriptorSets(context->device, Temp(VkDescriptorSetAllocateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = context->descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &context->heapLayout,
        }), &impl->set));

        for (u32 i = 0; i < 1024 * 1024; ++i)
            impl->generalFreelist.push_back(i);

        for (u32 i = 0; i < 4096; ++i)
            impl->samplerFreelist.push_back(i);

        return{ impl };
    }

    void DescriptorHeap::Destroy()
    {
        if (!impl) {
            return;
        }

        vkFreeDescriptorSets(impl->context->device, impl->context->descriptorPool, 1, &impl->set);

        delete impl;
        impl = nullptr;
    }

    DescriptorHandle DescriptorHeap::Acquire(DescriptorType type) const
    {
        auto* list = type == DescriptorType::Sampler
            ? &impl->samplerFreelist
            : &impl->generalFreelist;

        if (list->empty()) {
            NOVA_THROW("No more descriptor slots!");
        }

        u32 slot = list->back();
        list->pop_back();
        return { slot, type };
    }

    void DescriptorHeap::Release(DescriptorHandle handle) const
    {
        auto* list = DescriptorType(handle.type) == DescriptorType::Sampler
            ? &impl->samplerFreelist
            : &impl->generalFreelist;

        list->push_back(handle.id);
    }

// -----------------------------------------------------------------------------

    void CommandList::BindDescriptorHeap(BindPoint bindPoint, HDescriptorHeap heap) const
    {
        vkCmdBindDescriptorSets(impl->buffer, GetVulkanPipelineBindPoint(bindPoint),
            impl->pool->context->pipelineLayout, 0,
            1, &heap->set,
            0, nullptr);
    }

// -----------------------------------------------------------------------------

    DescriptorHandle DescriptorHeap::WriteStorageBuffer(DescriptorHandle handle, HBuffer buffer) const
    {
        vkUpdateDescriptorSets(impl->context->device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = impl->set,
            .dstBinding = 0,
            .dstArrayElement = handle.id,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = Temp(VkDescriptorBufferInfo {
                .buffer = buffer->buffer,
                .range = VK_WHOLE_SIZE,
            }),
        }), 0, nullptr);

        return handle;
    }

    DescriptorHandle DescriptorHeap::WriteUniformBuffer(DescriptorHandle handle, HBuffer buffer) const
    {
        vkUpdateDescriptorSets(impl->context->device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = impl->set,
            .dstBinding = 0,
            .dstArrayElement = handle.id,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = Temp(VkDescriptorBufferInfo {
                .buffer = buffer->buffer,
                .range = VK_WHOLE_SIZE,
            }),
        }), 0, nullptr);

        return handle;
    }

    DescriptorHandle DescriptorHeap::WriteSampledTexture(DescriptorHandle handle, HTexture texture) const
    {
        vkUpdateDescriptorSets(impl->context->device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = impl->set,
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

    DescriptorHandle DescriptorHeap::WriteSampler(DescriptorHandle handle, HSampler sampler) const
    {
        vkUpdateDescriptorSets(impl->context->device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = impl->set,
            .dstBinding = 1,
            .dstArrayElement = handle.id,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = Temp(VkDescriptorImageInfo {
                .sampler = sampler->sampler,
            }),
        }), 0, nullptr);

        return handle;
    }

    DescriptorHandle DescriptorHeap::WriteStorageTexture(DescriptorHandle handle, HTexture texture) const
    {
        vkUpdateDescriptorSets(impl->context->device, 1, Temp(VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = impl->set,
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