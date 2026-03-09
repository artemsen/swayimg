// SPDX-License-Identifier: MIT
// GPU blur for background effects (extend/mirror modes).
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "buildconf.hpp"

#ifdef HAVE_VULKAN

#include "vulkan_blur.hpp"
#include "vulkan_ctx.hpp"
#include "vulkan_pipeline.hpp"

#include "blur_comp_spv.h"

#include <algorithm>
#include <cmath>

constexpr int VulkanBlur::blur_radii[];

VulkanBlur& VulkanBlur::self()
{
    static VulkanBlur instance;
    return instance;
}

bool VulkanBlur::init(uint32_t width, uint32_t height)
{
    if (!create_compute_pipeline()) {
        return false;
    }
    if (!create_images(width, height)) {
        return false;
    }
    if (!create_descriptors()) {
        return false;
    }
    return true;
}

void VulkanBlur::destroy()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();
    if (dev == VK_NULL_HANDLE) {
        return;
    }

    destroy_images();

    if (blur_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, blur_pipeline, nullptr);
        blur_pipeline = VK_NULL_HANDLE;
    }
    if (compute_pipe_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, compute_pipe_layout, nullptr);
        compute_pipe_layout = VK_NULL_HANDLE;
    }
    if (compute_ds_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(dev, compute_ds_layout, nullptr);
        compute_ds_layout = VK_NULL_HANDLE;
    }
}

bool VulkanBlur::resize(uint32_t width, uint32_t height)
{
    if (width == img_width && height == img_height) {
        return true;
    }
    destroy_images();
    if (!create_images(width, height)) {
        return false;
    }
    return create_descriptors();
}

bool VulkanBlur::create_images(uint32_t width, uint32_t height)
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    img_width = width;
    img_height = height;

    const VkImageCreateInfo img_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { width, height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if (vkCreateImage(dev, &img_info, nullptr, &image_a) != VK_SUCCESS ||
        vkCreateImage(dev, &img_info, nullptr, &image_b) != VK_SUCCESS) {
        return false;
    }

    // Allocate device-local memory
    auto alloc_mem = [&](VkImage img, VkDeviceMemory& mem) -> bool {
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(dev, img, &req);
        uint32_t mem_type = ctx.find_memory_type(
            req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (mem_type == UINT32_MAX) {
            return false;
        }
        const VkMemoryAllocateInfo alloc = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = req.size,
            .memoryTypeIndex = mem_type,
        };
        if (vkAllocateMemory(dev, &alloc, nullptr, &mem) != VK_SUCCESS) {
            return false;
        }
        return vkBindImageMemory(dev, img, mem, 0) == VK_SUCCESS;
    };

    if (!alloc_mem(image_a, memory_a) || !alloc_mem(image_b, memory_b)) {
        return false;
    }

    // Create image views
    auto create_view = [&](VkImage img, VkImageView& view) -> bool {
        const VkImageViewCreateInfo vi = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = img,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        return vkCreateImageView(dev, &vi, nullptr, &view) == VK_SUCCESS;
    };

    if (!create_view(image_a, view_a) || !create_view(image_b, view_b)) {
        return false;
    }

    // Create sampler for sampling the result
    const VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };
    if (vkCreateSampler(dev, &sampler_info, nullptr,
                         &result_texture.sampler) != VK_SUCCESS) {
        return false;
    }

    // Allocate result descriptor set (combined image sampler for draw_image)
    VkDescriptorSetLayout tex_layout =
        VulkanPipeline::self().get_texture_layout();
    const VkDescriptorSetAllocateInfo ds_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ctx.get_descriptor_pool(),
        .descriptorSetCount = 1,
        .pSetLayouts = &tex_layout,
    };
    if (vkAllocateDescriptorSets(dev, &ds_alloc,
                                  &result_texture.descriptor) != VK_SUCCESS) {
        return false;
    }

    // Update result descriptor to point to image_a
    const VkDescriptorImageInfo result_img = {
        .sampler = result_texture.sampler,
        .imageView = view_a,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    const VkWriteDescriptorSet result_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = result_texture.descriptor,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &result_img,
    };
    vkUpdateDescriptorSets(dev, 1, &result_write, 0, nullptr);

    result_texture.width = width;
    result_texture.height = height;

    return true;
}

void VulkanBlur::destroy_images()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();
    if (dev == VK_NULL_HANDLE) {
        return;
    }

    // Free descriptor sets
    if (ds_a_to_b != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(dev, ctx.get_descriptor_pool(), 1, &ds_a_to_b);
        ds_a_to_b = VK_NULL_HANDLE;
    }
    if (ds_b_to_a != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(dev, ctx.get_descriptor_pool(), 1, &ds_b_to_a);
        ds_b_to_a = VK_NULL_HANDLE;
    }
    if (result_texture.descriptor != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(dev, ctx.get_descriptor_pool(), 1,
                              &result_texture.descriptor);
        result_texture.descriptor = VK_NULL_HANDLE;
    }
    if (result_texture.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, result_texture.sampler, nullptr);
        result_texture.sampler = VK_NULL_HANDLE;
    }

    auto destroy_img = [&](VkImage& img, VkDeviceMemory& mem,
                           VkImageView& view) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(dev, view, nullptr);
            view = VK_NULL_HANDLE;
        }
        if (img != VK_NULL_HANDLE) {
            vkDestroyImage(dev, img, nullptr);
            img = VK_NULL_HANDLE;
        }
        if (mem != VK_NULL_HANDLE) {
            vkFreeMemory(dev, mem, nullptr);
            mem = VK_NULL_HANDLE;
        }
    };

    destroy_img(image_a, memory_a, view_a);
    destroy_img(image_b, memory_b, view_b);

    img_width = 0;
    img_height = 0;
}

bool VulkanBlur::create_compute_pipeline()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    // Descriptor set layout: 2 storage images
    const VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };

    const VkDescriptorSetLayoutCreateInfo ds_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings,
    };

    if (vkCreateDescriptorSetLayout(dev, &ds_layout_info, nullptr,
                                     &compute_ds_layout) != VK_SUCCESS) {
        return false;
    }

    // Push constants: direction (int) + radius (int) = 8 bytes
    const VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = 8,
    };

    const VkPipelineLayoutCreateInfo pipe_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &compute_ds_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    };

    if (vkCreatePipelineLayout(dev, &pipe_layout_info, nullptr,
                                &compute_pipe_layout) != VK_SUCCESS) {
        return false;
    }

    // Create shader module
    const VkShaderModuleCreateInfo shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = blur_comp_spv_len,
        .pCode = reinterpret_cast<const uint32_t*>(blur_comp_spv),
    };

    VkShaderModule shader;
    if (vkCreateShaderModule(dev, &shader_info, nullptr, &shader) !=
        VK_SUCCESS) {
        return false;
    }

    const VkComputePipelineCreateInfo pipe_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName = "main",
        },
        .layout = compute_pipe_layout,
    };

    VkResult result = vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1,
                                                &pipe_info, nullptr,
                                                &blur_pipeline);

    vkDestroyShaderModule(dev, shader, nullptr);
    return result == VK_SUCCESS;
}

bool VulkanBlur::create_descriptors()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    // Allocate descriptor sets for both directions
    VkDescriptorSetLayout layouts[] = { compute_ds_layout, compute_ds_layout };
    VkDescriptorSet sets[2];

    const VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ctx.get_descriptor_pool(),
        .descriptorSetCount = 2,
        .pSetLayouts = layouts,
    };

    if (vkAllocateDescriptorSets(dev, &alloc_info, sets) != VK_SUCCESS) {
        return false;
    }

    ds_a_to_b = sets[0];
    ds_b_to_a = sets[1];

    // A→B: read from A (binding 0), write to B (binding 1)
    const VkDescriptorImageInfo a_info = {
        .imageView = view_a,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    const VkDescriptorImageInfo b_info = {
        .imageView = view_b,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    const VkWriteDescriptorSet writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds_a_to_b,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &a_info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds_a_to_b,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &b_info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds_b_to_a,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &b_info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds_b_to_a,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &a_info,
        },
    };

    vkUpdateDescriptorSets(dev, 4, writes, 0, nullptr);
    return true;
}

void VulkanBlur::prepare(VkCommandBuffer cmd, const GpuTexture& source)
{
    // Calculate source crop for scaled-to-fill
    const float src_w = static_cast<float>(source.width);
    const float src_h = static_cast<float>(source.height);
    const float dst_w = static_cast<float>(img_width);
    const float dst_h = static_cast<float>(img_height);
    const float fill_scale = std::max(dst_w / src_w, dst_h / src_h);

    // Visible source region (centered crop that fills the destination)
    const float vis_w = dst_w / fill_scale;
    const float vis_h = dst_h / fill_scale;
    const float src_x = (src_w - vis_w) / 2.0f;
    const float src_y = (src_h - vis_h) / 2.0f;

    // Transition source texture to TRANSFER_SRC
    // Transition image_a to TRANSFER_DST
    VkImageMemoryBarrier barriers[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = source.image,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image_a,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        },
    };

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2,
        barriers);

    // Blit source (centered crop) scaled-to-fill into image_a
    const VkImageBlit blit = {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets = {
            { static_cast<int32_t>(src_x), static_cast<int32_t>(src_y), 0 },
            { static_cast<int32_t>(src_x + vis_w),
              static_cast<int32_t>(src_y + vis_h), 1 },
        },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets = {
            { 0, 0, 0 },
            { static_cast<int32_t>(img_width),
              static_cast<int32_t>(img_height), 1 },
        },
    };

    vkCmdBlitImage(cmd, source.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image_a, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    // Transition source back to SHADER_READ_ONLY
    // Transition image_a to GENERAL for compute
    // Transition image_b to GENERAL for compute
    VkImageMemoryBarrier post_barriers[3] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = source.image,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                             VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image_a,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image_b,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        },
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 3, post_barriers);
}

void VulkanBlur::apply(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, blur_pipeline);

    const uint32_t group_x = (img_width + 15) / 16;
    const uint32_t group_y = (img_height + 15) / 16;

    struct {
        int direction;
        int radius;
    } pc;

    for (int pass = 0; pass < BLUR_PASSES; ++pass) {
        // Horizontal: A → B
        pc = { 0, blur_radii[pass] };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                compute_pipe_layout, 0, 1, &ds_a_to_b, 0,
                                nullptr);
        vkCmdPushConstants(cmd, compute_pipe_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
        vkCmdDispatch(cmd, group_x, group_y, 1);

        // Memory barrier between passes
        const VkMemoryBarrier mem_barrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &mem_barrier, 0, nullptr, 0, nullptr);

        // Vertical: B → A
        pc = { 1, blur_radii[pass] };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                compute_pipe_layout, 0, 1, &ds_b_to_a, 0,
                                nullptr);
        vkCmdPushConstants(cmd, compute_pipe_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
        vkCmdDispatch(cmd, group_x, group_y, 1);

        // Barrier if not last pass
        if (pass < BLUR_PASSES - 1) {
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                                 &mem_barrier, 0, nullptr, 0, nullptr);
        }
    }

    // Transition image_a to SHADER_READ_ONLY for sampling in render pass
    const VkImageMemoryBarrier final_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image_a,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &final_barrier);
}

#endif // HAVE_VULKAN
