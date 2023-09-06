#include "nova_VulkanRHI.hpp"

namespace nova
{
    DescriptorHeap DescriptorHeap::Create(HContext context, u32 requestedDescriptorCount)
    {
        auto impl = new Impl;
        impl->context = context;

        impl->descriptorCount = std::min(requestedDescriptorCount, context->maxDescriptors);

        if (impl->context->descriptorBuffers) {
            impl->descriptorBuffer = nova::Buffer::Create(context,
                context->mutableDescriptorSize * impl->descriptorCount,
                nova::BufferUsage::DescriptorResources | nova::BufferUsage::DescriptorSamplers,
                nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);

        } else {
            vkh::Check(vkCreateDescriptorPool(context->device, Temp(VkDescriptorPoolCreateInfo {
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
                .maxSets = impl->descriptorCount,
                .poolSizeCount = 1,
                .pPoolSizes = std::array {
                    VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLER, impl->descriptorCount },
                }.data(),
            }), context->pAlloc, &impl->descriptorPool));

            vkh::Check(vkAllocateDescriptorSets(context->device, Temp(VkDescriptorSetAllocateInfo {
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
        }

        return{ impl };
    }

    void DescriptorHeap::Destroy()
    {
        if (!impl) {
            return;
        }

        if (impl->context->descriptorBuffers) {
            impl->descriptorBuffer.Destroy();
        } else {
            vkDestroyDescriptorPool(impl->context->device, impl->descriptorPool, impl->context->pAlloc);
        }

        delete impl;
        impl = nullptr;
    }

    u32 DescriptorHeap::GetMaxDescriptorCount() const
    {
        return impl->descriptorCount;
    }

    void CommandList::BindDescriptorHeap(BindPoint bindPoint, HDescriptorHeap heap) const
    {
        if (heap->context->descriptorBuffers) {
            vkCmdBindDescriptorBuffersEXT(impl->buffer, 1, Temp(VkDescriptorBufferBindingInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                .address = heap->descriptorBuffer.GetAddress(),
                .usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
                    | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
            }));
            vkCmdSetDescriptorBufferOffsetsEXT(impl->buffer, GetVulkanPipelineBindPoint(bindPoint),
                heap->context->pipelineLayout,
                0, 1, Temp(0u), Temp(0ull));
        } else {
            vkCmdBindDescriptorSets(impl->buffer, GetVulkanPipelineBindPoint(bindPoint),
                heap->context->pipelineLayout, 0,
                1, std::array {
                    heap->descriptorSet,
                }.data(),
                0, nullptr);
        }
    }

// -----------------------------------------------------------------------------
//                              Storage Buffers
// -----------------------------------------------------------------------------

    void DescriptorHeap::WriteStorageBuffer(DescriptorHandle handle, HBuffer buffer, u64 size, u64 offset) const
    {
        if (impl->context->descriptorBuffers) {
            auto descriptorSize = impl->context->descriptorSizes.storageBufferDescriptorSize;
            vkGetDescriptorEXT(impl->context->device, Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .data{
                    .pStorageBuffer = Temp(VkDescriptorAddressInfoEXT {
                        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
                        .address = buffer.Unwrap().GetAddress() + offset,
                        .range = size == ~0ull ? (buffer.Unwrap().GetSize() - offset) : size,
                    }),
                }
            }), descriptorSize, impl->descriptorBuffer.GetMapped() + impl->context->mutableDescriptorSize * handle.id);
        } else {
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
        }
    }

    void CommandList::WriteStorageBuffer(DescriptorHeap heap, DescriptorHandle handle, HBuffer buffer, u64 size, u64 offset) const
    {
        auto descriptorSize = heap->context->descriptorSizes.storageBufferDescriptorSize;
        auto data = NOVA_ALLOC_STACK(char, descriptorSize);

        vkGetDescriptorEXT(heap->context->device, Temp(VkDescriptorGetInfoEXT {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .data{
                .pStorageBuffer = Temp(VkDescriptorAddressInfoEXT {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
                    .address = buffer.Unwrap().GetAddress() + offset,
                    .range = size == ~0ull ? (buffer.Unwrap().GetSize() - offset) : size,
                }),
            }
        }), descriptorSize, data);

        vkCmdUpdateBuffer(impl->buffer,
            heap->descriptorBuffer->buffer,
            heap->context->mutableDescriptorSize * handle.id,
            descriptorSize,
            data);
    }

// -----------------------------------------------------------------------------
//                              Uniform Buffers
// -----------------------------------------------------------------------------

    void DescriptorHeap::WriteUniformBuffer(DescriptorHandle handle, HBuffer buffer, u64 size, u64 offset) const
    {
        if (impl->context->descriptorBuffers) {
            auto descriptorSize = impl->context->descriptorSizes.uniformBufferDescriptorSize;
            vkGetDescriptorEXT(impl->context->device, Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .data{
                    .pUniformBuffer = Temp(VkDescriptorAddressInfoEXT {
                        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
                        .address = buffer.Unwrap().GetAddress() + offset,
                        .range = size == ~0ull ? (buffer.Unwrap().GetSize() - offset) : size,
                    }),
                }
            }), descriptorSize, impl->descriptorBuffer.GetMapped() + impl->context->mutableDescriptorSize * handle.id);
        } else {
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
        }
    }

    void CommandList::WriteUniformBuffer(DescriptorHeap heap, DescriptorHandle handle, HBuffer buffer, u64 size, u64 offset) const
    {
        auto descriptorSize = heap->context->descriptorSizes.uniformBufferDescriptorSize;
        auto data = NOVA_ALLOC_STACK(char, descriptorSize);

        vkGetDescriptorEXT(heap->context->device, Temp(VkDescriptorGetInfoEXT {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .data{
                .pUniformBuffer = Temp(VkDescriptorAddressInfoEXT {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
                    .address = buffer.Unwrap().GetAddress() + offset,
                    .range = size == ~0ull ? (buffer.Unwrap().GetSize() - offset) : size,
                }),
            }
        }), descriptorSize, data);

        vkCmdUpdateBuffer(impl->buffer,
            heap->descriptorBuffer->buffer,
            heap->context->mutableDescriptorSize * handle.id,
            descriptorSize,
            data);
    }

// -----------------------------------------------------------------------------
//                              Sampled Textures
// -----------------------------------------------------------------------------

    void DescriptorHeap::WriteSampledTexture(DescriptorHandle handle, HTexture texture) const
    {
        if (impl->context->descriptorBuffers) {
            auto descriptorSize = impl->context->descriptorSizes.sampledImageDescriptorSize;
            vkGetDescriptorEXT(impl->context->device, Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .data{
                    .pSampledImage = Temp(VkDescriptorImageInfo {
                        .imageView = texture->view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    }),
                }
            }), descriptorSize, impl->descriptorBuffer.GetMapped() + impl->context->mutableDescriptorSize * handle.id);
        } else {
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
        }
    }

    void CommandList::WriteSampledTexture(DescriptorHeap heap, DescriptorHandle handle, HTexture texture) const
    {
        auto descriptorSize = heap->context->descriptorSizes.sampledImageDescriptorSize;
        auto data = NOVA_ALLOC_STACK(char, descriptorSize);

        vkGetDescriptorEXT(heap->context->device, Temp(VkDescriptorGetInfoEXT {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .data{
                .pSampledImage = Temp(VkDescriptorImageInfo {
                    .imageView = texture->view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                }),
            }
        }), descriptorSize, data);

        vkCmdUpdateBuffer(impl->buffer,
            heap->descriptorBuffer->buffer,
            heap->context->mutableDescriptorSize * handle.id,
            descriptorSize,
            data);
    }

// -----------------------------------------------------------------------------
//                              Storage Textures
// -----------------------------------------------------------------------------

    void DescriptorHeap::WriteStorageTexture(DescriptorHandle handle, HTexture texture) const
    {
        if (impl->context->descriptorBuffers) {
            auto descriptorSize = impl->context->descriptorSizes.storageImageDescriptorSize;
            vkGetDescriptorEXT(impl->context->device, Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .data{
                    .pStorageImage = Temp(VkDescriptorImageInfo {
                        .imageView = texture->view,
                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                    }),
                }
            }), descriptorSize, impl->descriptorBuffer.GetMapped() + impl->context->mutableDescriptorSize * handle.id);
        } else {
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
        }
    }

    void CommandList::WriteStorageTexture(DescriptorHeap heap, DescriptorHandle handle, HTexture texture) const
    {
        auto descriptorSize = heap->context->descriptorSizes.storageImageDescriptorSize;
        auto data = NOVA_ALLOC_STACK(char, descriptorSize);

        vkGetDescriptorEXT(heap->context->device, Temp(VkDescriptorGetInfoEXT {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .data{
                .pStorageImage = Temp(VkDescriptorImageInfo {
                    .imageView = texture->view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                }),
            }
        }), descriptorSize, data);

        vkCmdUpdateBuffer(impl->buffer,
            heap->descriptorBuffer->buffer,
            heap->context->mutableDescriptorSize * handle.id,
            descriptorSize,
            data);
    }

// -----------------------------------------------------------------------------
//                                 Samplers
// -----------------------------------------------------------------------------

    void DescriptorHeap::WriteSampler(DescriptorHandle handle, HSampler sampler) const
    {
        if (impl->context->descriptorBuffers) {
            auto descriptorSize = impl->context->descriptorSizes.samplerDescriptorSize;
            vkGetDescriptorEXT(impl->context->device, Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                .data{
                    .pSampler = &sampler->sampler,
                }
            }), descriptorSize, impl->descriptorBuffer.GetMapped() + impl->context->mutableDescriptorSize * handle.id);
        } else {
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
        }
    }

    void CommandList::WriteSampler(DescriptorHeap heap, DescriptorHandle handle, HSampler sampler) const
    {
        auto descriptorSize = heap->context->descriptorSizes.samplerDescriptorSize;
        auto data = NOVA_ALLOC_STACK(char, descriptorSize);

        vkGetDescriptorEXT(heap->context->device, Temp(VkDescriptorGetInfoEXT {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .data{
                .pSampler = &sampler->sampler,
            }
        }), descriptorSize, data);

        vkCmdUpdateBuffer(impl->buffer,
            heap->descriptorBuffer->buffer,
            heap->context->mutableDescriptorSize * handle.id,
            descriptorSize,
            data);
    }

// -----------------------------------------------------------------------------
//                          Acceleration Structures
// -----------------------------------------------------------------------------

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