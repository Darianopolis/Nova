#include "nova_VulkanRHI.hpp"

namespace nova
{
    void DescriptorHeap::Init(HContext _context, u32 _imageDescriptorCount, u32 _samplerDescriptorCount)
    {
        context = _context;
        imageDescriptorCount = _imageDescriptorCount;
        samplerDescriptorCount = _samplerDescriptorCount;

        VkDescriptorSetLayoutCreateFlags bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        if (!context->descriptorBuffers) bindingFlags |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        vkh::Check(context->vkCreateDescriptorSetLayout(context->device, Temp(VkDescriptorSetLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = Temp(VkDescriptorSetLayoutBindingFlagsCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = 3,
                .pBindingFlags = std::array { bindingFlags, bindingFlags, bindingFlags }.data(),
            }),
            .flags = VkDescriptorSetLayoutCreateFlags(context->descriptorBuffers
                ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                : VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT),
            .bindingCount = 3,
            .pBindings = std::array {
                VkDescriptorSetLayoutBinding {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    .descriptorCount = imageDescriptorCount,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                },
                VkDescriptorSetLayoutBinding {
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .descriptorCount = imageDescriptorCount,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                },
                VkDescriptorSetLayoutBinding {
                    .binding = 2,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                    .descriptorCount = samplerDescriptorCount,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                },
            }.data(),
        }), context->pAlloc, &heapLayout));

        vkh::Check(context->vkCreatePipelineLayout(context->device, Temp(VkPipelineLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &heapLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = Temp(VkPushConstantRange {
                .stageFlags = VK_SHADER_STAGE_ALL,
                .size = 256,
            }),
        }), context->pAlloc, &pipelineLayout));

        if (context->descriptorBuffers) {
            VkDeviceSize size;
            context->vkGetDescriptorSetLayoutSizeEXT(context->device, heapLayout, &size);

            context->vkGetDescriptorSetLayoutBindingOffsetEXT(context->device, heapLayout, 0, &sampledOffset);
            context->vkGetDescriptorSetLayoutBindingOffsetEXT(context->device, heapLayout, 1, &storageOffset);
            context->vkGetDescriptorSetLayoutBindingOffsetEXT(context->device, heapLayout, 2, &samplerOffset);

            sampledStride = context->descriptorSizes.sampledImageDescriptorSize;
            storageStride = context->descriptorSizes.storageImageDescriptorSize;
            samplerStride = context->descriptorSizes.samplerDescriptorSize;

            descriptorBuffer = nova::Buffer::Create(context, size,
                nova::BufferUsage::DescriptorResources | nova::BufferUsage::DescriptorSamplers,
                nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);

        } else {
            vkh::Check(context->vkCreateDescriptorPool(context->device, Temp(VkDescriptorPoolCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                .maxSets = 1,
                .poolSizeCount = 3,
                .pPoolSizes = std::array {
                    VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageDescriptorCount },
                    VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageDescriptorCount },
                    VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLER, samplerDescriptorCount },
                }.data(),
            }), context->pAlloc, &descriptorPool));

            vkh::Check(context->vkAllocateDescriptorSets(context->device, Temp(VkDescriptorSetAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &heapLayout,
            }), &descriptorSet));
        }
    }

    void DescriptorHeap::Destroy()
    {
        if (context->descriptorBuffers) {
            descriptorBuffer.Destroy();
        } else {
            context->vkDestroyDescriptorPool(context->device, descriptorPool, context->pAlloc);
        }

        context->vkDestroyPipelineLayout(context->device, pipelineLayout, context->pAlloc);
        context->vkDestroyDescriptorSetLayout(context->device, heapLayout, context->pAlloc);
    }

    void DescriptorHeap::Bind(CommandList cmd, BindPoint bindPoint)
    {
        if (context->descriptorBuffers) {
            context->vkCmdBindDescriptorBuffersEXT(cmd->buffer, 1, Temp(VkDescriptorBufferBindingInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                .address = descriptorBuffer.GetAddress(),
                .usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
                    | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
            }));
            context->vkCmdSetDescriptorBufferOffsetsEXT(cmd->buffer, GetVulkanPipelineBindPoint(bindPoint),
                pipelineLayout,
                0, 1, Temp(0u), Temp(0ull));
        } else {
            context->vkCmdBindDescriptorSets(cmd->buffer, GetVulkanPipelineBindPoint(bindPoint),
                pipelineLayout, 0,
                1, &descriptorSet,
                0, nullptr);
        }
    }

// -----------------------------------------------------------------------------
//                                Textures
// -----------------------------------------------------------------------------

    void DescriptorHeap::WriteSampled(u32 index, HTexture texture)
    {
        if (context->descriptorBuffers) {
            context->vkGetDescriptorEXT(context->device, Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .data{
                    .pSampledImage = Temp(VkDescriptorImageInfo {
                        .imageView = texture->view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    }),
                }
            }), sampledStride, descriptorBuffer.GetMapped() + sampledOffset + sampledStride * index);
        } else {
            context->vkUpdateDescriptorSets(context->device, 1, std::array {
                VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptorSet,
                    .dstBinding = 0,
                    .dstArrayElement = index,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    .pImageInfo = Temp(VkDescriptorImageInfo {
                        .imageView = texture->view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    }),
                },
            }.data(), 0, nullptr);
        }
    }

    void DescriptorHeap::WriteStorage(u32 index, HTexture texture)
    {
        if (context->descriptorBuffers) {
            context->vkGetDescriptorEXT(context->device, Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .data{
                    .pStorageImage = Temp(VkDescriptorImageInfo {
                        .imageView = texture->view,
                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                    }),
                }
            }), storageStride, descriptorBuffer.GetMapped() + storageOffset + storageStride * index);
        } else {
            context->vkUpdateDescriptorSets(context->device, 1, Temp(VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSet,
                .dstBinding = 1,
                .dstArrayElement = index,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = Temp(VkDescriptorImageInfo {
                    .imageView = texture->view,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                }),
            }), 0, nullptr);
        }
    }

// -----------------------------------------------------------------------------
//                                 Samplers
// -----------------------------------------------------------------------------

    void DescriptorHeap::WriteSampler(u32 index, HSampler sampler)
    {
        if (context->descriptorBuffers) {
            context->vkGetDescriptorEXT(context->device, Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                .data{
                    .pSampler = &sampler->sampler,
                }
            }), samplerStride, descriptorBuffer.GetMapped() + samplerOffset + samplerStride * index);
        } else {
            context->vkUpdateDescriptorSets(context->device, 1, Temp(VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSet,
                .dstBinding = 2,
                .dstArrayElement = index,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .pImageInfo = Temp(VkDescriptorImageInfo {
                    .sampler = sampler->sampler,
                }),
            }), 0, nullptr);
        }
    }
}