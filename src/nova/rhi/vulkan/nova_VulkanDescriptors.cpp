#include "nova_VulkanRHI.hpp"

namespace nova
{
    void DescriptorHeap::Init(HContext _context, u32 _image_descriptor_count, u32 _sampler_descriptor_count)
    {
        context = _context;
        image_descriptor_count = _image_descriptor_count;
        sampler_descriptor_count = _sampler_descriptor_count;

        VkDescriptorSetLayoutCreateFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        if (!context->descriptor_buffers) binding_flags |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        vkh::Check(context->vkCreateDescriptorSetLayout(context->device, Temp(VkDescriptorSetLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = Temp(VkDescriptorSetLayoutBindingFlagsCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = 3,
                .pBindingFlags = std::array { binding_flags, binding_flags, binding_flags }.data(),
            }),
            .flags = VkDescriptorSetLayoutCreateFlags(context->descriptor_buffers
                ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                : VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT),
            .bindingCount = 3,
            .pBindings = std::array {
                VkDescriptorSetLayoutBinding {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    .descriptorCount = image_descriptor_count,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                },
                VkDescriptorSetLayoutBinding {
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .descriptorCount = image_descriptor_count,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                },
                VkDescriptorSetLayoutBinding {
                    .binding = 2,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                    .descriptorCount = sampler_descriptor_count,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                },
            }.data(),
        }), context->alloc, &heap_layout));

        vkh::Check(context->vkCreatePipelineLayout(context->device, Temp(VkPipelineLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &heap_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = Temp(VkPushConstantRange {
                .stageFlags = VK_SHADER_STAGE_ALL,
                .size = context->push_constant_size,
            }),
        }), context->alloc, &pipeline_layout));

        if (context->descriptor_buffers) {
            VkDeviceSize size;
            context->vkGetDescriptorSetLayoutSizeEXT(context->device, heap_layout, &size);

            context->vkGetDescriptorSetLayoutBindingOffsetEXT(context->device, heap_layout, 0, &sampled_offset);
            context->vkGetDescriptorSetLayoutBindingOffsetEXT(context->device, heap_layout, 1, &storage_offset);
            context->vkGetDescriptorSetLayoutBindingOffsetEXT(context->device, heap_layout, 2, &sampler_offset);

            sampled_stride = context->descriptor_sizes.sampledImageDescriptorSize;
            storage_stride = context->descriptor_sizes.storageImageDescriptorSize;
            sampler_stride = context->descriptor_sizes.samplerDescriptorSize;

            descriptor_buffer = nova::Buffer::Create(context, size,
                nova::BufferUsage::DescriptorResources | nova::BufferUsage::DescriptorSamplers,
                nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);

        } else {
            vkh::Check(context->vkCreateDescriptorPool(context->device, Temp(VkDescriptorPoolCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                .maxSets = 1,
                .poolSizeCount = 3,
                .pPoolSizes = std::array {
                    VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, image_descriptor_count },
                    VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, image_descriptor_count },
                    VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLER, sampler_descriptor_count },
                }.data(),
            }), context->alloc, &descriptor_pool));

            vkh::Check(context->vkAllocateDescriptorSets(context->device, Temp(VkDescriptorSetAllocateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = descriptor_pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &heap_layout,
            }), &descriptor_set));
        }
    }

    void DescriptorHeap::Destroy()
    {
        if (context->descriptor_buffers) {
            descriptor_buffer.Destroy();
        } else {
            context->vkDestroyDescriptorPool(context->device, descriptor_pool, context->alloc);
        }

        context->vkDestroyPipelineLayout(context->device, pipeline_layout, context->alloc);
        context->vkDestroyDescriptorSetLayout(context->device, heap_layout, context->alloc);
    }

    void DescriptorHeap::Bind(CommandList cmd, BindPoint bind_point)
    {
        if (context->descriptor_buffers) {
            context->vkCmdBindDescriptorBuffersEXT(cmd->buffer, 1, Temp(VkDescriptorBufferBindingInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                .address = descriptor_buffer.GetAddress(),
                .usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
                    | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
            }));
            context->vkCmdSetDescriptorBufferOffsetsEXT(cmd->buffer, GetVulkanPipelineBindPoint(bind_point),
                pipeline_layout,
                0, 1, Temp(0u), Temp(0ull));
        } else {
            context->vkCmdBindDescriptorSets(cmd->buffer, GetVulkanPipelineBindPoint(bind_point),
                pipeline_layout, 0,
                1, &descriptor_set,
                0, nullptr);
        }
    }

// -----------------------------------------------------------------------------
//                                 Images
// -----------------------------------------------------------------------------

    void DescriptorHeap::WriteSampled(u32 index, HImage image)
    {
        if (context->descriptor_buffers) {
            context->vkGetDescriptorEXT(context->device, Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .data{
                    .pSampledImage = Temp(VkDescriptorImageInfo {
                        .imageView = image->view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    }),
                }
            }), sampled_stride, descriptor_buffer.GetMapped() + sampled_offset + sampled_stride * index);
        } else {
            context->vkUpdateDescriptorSets(context->device, 1, std::array {
                VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_set,
                    .dstBinding = 0,
                    .dstArrayElement = index,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    .pImageInfo = Temp(VkDescriptorImageInfo {
                        .imageView = image->view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    }),
                },
            }.data(), 0, nullptr);
        }
    }

    void DescriptorHeap::WriteStorage(u32 index, HImage image)
    {
        if (context->descriptor_buffers) {
            context->vkGetDescriptorEXT(context->device, Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .data{
                    .pStorageImage = Temp(VkDescriptorImageInfo {
                        .imageView = image->view,
                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                    }),
                }
            }), storage_stride, descriptor_buffer.GetMapped() + storage_offset + storage_stride * index);
        } else {
            context->vkUpdateDescriptorSets(context->device, 1, Temp(VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptor_set,
                .dstBinding = 1,
                .dstArrayElement = index,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = Temp(VkDescriptorImageInfo {
                    .imageView = image->view,
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
        if (context->descriptor_buffers) {
            context->vkGetDescriptorEXT(context->device, Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                .data{
                    .pSampler = &sampler->sampler,
                }
            }), sampler_stride, descriptor_buffer.GetMapped() + sampler_offset + sampler_stride * index);
        } else {
            context->vkUpdateDescriptorSets(context->device, 1, Temp(VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptor_set,
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