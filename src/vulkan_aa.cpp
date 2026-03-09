// SPDX-License-Identifier: MIT
// GPU-accelerated MKS13 anti-aliased image scaling.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "buildconf.hpp"

#ifdef HAVE_VULKAN

#include "vulkan_aa.hpp"
#include "vulkan_ctx.hpp"
#include "vulkan_pipeline.hpp"

#include "mks13_comp_spv.h"

#include <algorithm>
#include <cmath>

static constexpr float WINDOW_SIZE = 2.5f;

VulkanAA& VulkanAA::self()
{
    static VulkanAA instance;
    return instance;
}

bool VulkanAA::init()
{
    return create_pipeline();
}

void VulkanAA::destroy()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();
    if (dev == VK_NULL_HANDLE) {
        return;
    }

    destroy_images();

    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipe_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, pipe_layout, nullptr);
        pipe_layout = VK_NULL_HANDLE;
    }
    if (ds_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(dev, ds_layout, nullptr);
        ds_layout = VK_NULL_HANDLE;
    }
}

bool VulkanAA::create_pipeline()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    // Descriptor set layout: sampler2D (input) + storage image (output)
    const VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
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
                                     &ds_layout) != VK_SUCCESS) {
        return false;
    }

    // Push constants: direction(4) + scale(4) + src_size(4) +
    //                 local_offset(4) + src_start(4) = 20 bytes
    const VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = 20,
    };

    const VkPipelineLayoutCreateInfo pipe_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ds_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    };

    if (vkCreatePipelineLayout(dev, &pipe_layout_info, nullptr,
                                &pipe_layout) != VK_SUCCESS) {
        return false;
    }

    // Create shader module
    const VkShaderModuleCreateInfo shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = mks13_comp_spv_len,
        .pCode = reinterpret_cast<const uint32_t*>(mks13_comp_spv),
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
        .layout = pipe_layout,
    };

    VkResult result = vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1,
                                                &pipe_info, nullptr, &pipeline);

    vkDestroyShaderModule(dev, shader, nullptr);
    return result == VK_SUCCESS;
}

bool VulkanAA::ensure_images(uint32_t new_inter_w, uint32_t new_inter_h,
                              uint32_t new_out_w, uint32_t new_out_h)
{
    // Check if existing images are large enough
    if (inter_image != VK_NULL_HANDLE && out_image != VK_NULL_HANDLE &&
        inter_w >= new_inter_w && inter_h >= new_inter_h &&
        out_w >= new_out_w && out_h >= new_out_h) {
        return true;
    }

    destroy_images();

    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    // Use requested sizes (grow only, sizes are computed fresh each frame)
    inter_w = new_inter_w;
    inter_h = new_inter_h;
    out_w = new_out_w;
    out_h = new_out_h;

    // Helper to create image + memory + view
    auto create_img = [&](uint32_t w, uint32_t h, VkImage& img,
                          VkDeviceMemory& mem, VkImageView& view) -> bool {
        const VkImageCreateInfo img_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = { w, h, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        if (vkCreateImage(dev, &img_info, nullptr, &img) != VK_SUCCESS) {
            return false;
        }

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
        if (vkBindImageMemory(dev, img, mem, 0) != VK_SUCCESS) {
            return false;
        }

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

    if (!create_img(inter_w, inter_h, inter_image, inter_memory, inter_view) ||
        !create_img(out_w, out_h, out_image, out_memory, out_view)) {
        destroy_images();
        return false;
    }

    // Create sampler for intermediate (used as input in vertical pass)
    const VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };
    if (vkCreateSampler(dev, &sampler_info, nullptr, &inter_sampler) !=
        VK_SUCCESS) {
        destroy_images();
        return false;
    }

    // Allocate descriptor sets
    VkDescriptorSetLayout layouts[] = { ds_layout, ds_layout };
    VkDescriptorSet sets[2];
    const VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ctx.get_descriptor_pool(),
        .descriptorSetCount = 2,
        .pSetLayouts = layouts,
    };
    if (vkAllocateDescriptorSets(dev, &alloc_info, sets) != VK_SUCCESS) {
        destroy_images();
        return false;
    }
    ds_hor = sets[0];
    ds_ver = sets[1];

    // Vertical pass descriptor: intermediate as sampler, output as storage
    const VkDescriptorImageInfo ver_sampler_info = {
        .sampler = inter_sampler,
        .imageView = inter_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    const VkDescriptorImageInfo ver_storage_info = {
        .imageView = out_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    const VkWriteDescriptorSet ver_writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds_ver,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ver_sampler_info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds_ver,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &ver_storage_info,
        },
    };
    vkUpdateDescriptorSets(dev, 2, ver_writes, 0, nullptr);

    // Result texture (output image as combined image sampler for draw_image)
    VkDescriptorSetLayout tex_layout =
        VulkanPipeline::self().get_texture_layout();
    const VkDescriptorSetAllocateInfo result_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ctx.get_descriptor_pool(),
        .descriptorSetCount = 1,
        .pSetLayouts = &tex_layout,
    };
    if (vkAllocateDescriptorSets(dev, &result_alloc,
                                  &result_texture.descriptor) != VK_SUCCESS) {
        destroy_images();
        return false;
    }

    // Create result sampler (nearest — AA already handled by compute)
    if (result_texture.sampler == VK_NULL_HANDLE) {
        if (vkCreateSampler(dev, &sampler_info, nullptr,
                             &result_texture.sampler) != VK_SUCCESS) {
            destroy_images();
            return false;
        }
    }

    const VkDescriptorImageInfo result_img = {
        .sampler = result_texture.sampler,
        .imageView = out_view,
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

    return true;
}

void VulkanAA::destroy_images()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();
    if (dev == VK_NULL_HANDLE) {
        return;
    }

    if (ds_hor != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(dev, ctx.get_descriptor_pool(), 1, &ds_hor);
        ds_hor = VK_NULL_HANDLE;
    }
    if (ds_ver != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(dev, ctx.get_descriptor_pool(), 1, &ds_ver);
        ds_ver = VK_NULL_HANDLE;
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
    if (inter_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, inter_sampler, nullptr);
        inter_sampler = VK_NULL_HANDLE;
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

    destroy_img(inter_image, inter_memory, inter_view);
    destroy_img(out_image, out_memory, out_view);

    inter_w = inter_h = out_w = out_h = 0;
}

VulkanAA::Result VulkanAA::apply(VkCommandBuffer cmd,
                                  const GpuTexture& source, float scale,
                                  float pos_x, float pos_y, uint32_t wnd_w,
                                  uint32_t wnd_h)
{
    Result result;

    if (!is_valid() || scale <= 0.0f) {
        return result;
    }

    // Compute visible output region (intersection of scaled image with window)
    const float img_w = static_cast<float>(source.width) * scale;
    const float img_h = static_cast<float>(source.height) * scale;

    const float vis_x = std::max(pos_x, 0.0f);
    const float vis_y = std::max(pos_y, 0.0f);
    const float vis_r = std::min(pos_x + img_w, static_cast<float>(wnd_w));
    const float vis_b = std::min(pos_y + img_h, static_cast<float>(wnd_h));

    if (vis_r <= vis_x || vis_b <= vis_y) {
        return result; // not visible
    }

    const auto visible_w = static_cast<uint32_t>(std::ceil(vis_r - vis_x));
    const auto visible_h = static_cast<uint32_t>(std::ceil(vis_b - vis_y));

    if (visible_w == 0 || visible_h == 0) {
        return result;
    }

    // Compute local offsets (maps output pixel 0 to image-relative coords)
    const float local_offset_x =
        std::max(0.0f, -pos_x); // pixels from image left edge
    const float local_offset_y = std::max(0.0f, -pos_y);

    // Compute source rows needed for vertical pass
    // The vertical kernel for the first visible output row needs rows around:
    //   c = (local_offset_y + 0.5) / scale - 0.5
    // And for the last:
    //   c = (local_offset_y + visible_h - 0.5) / scale - 0.5
    // With kernel window extending ±WINDOW_SIZE/min(scale,1)

    const float d = WINDOW_SIZE / std::min(scale, 1.0f);
    const float c_first = (local_offset_y + 0.5f) / scale - 0.5f;
    const float c_last =
        (local_offset_y + static_cast<float>(visible_h) - 0.5f) / scale - 0.5f;

    const int src_y_first =
        std::max(0, static_cast<int>(std::floor(c_first - d)));
    const int src_y_last = std::min(static_cast<int>(source.height) - 1,
                                     static_cast<int>(std::ceil(c_last + d)));
    const auto needed_rows =
        static_cast<uint32_t>(src_y_last - src_y_first + 1);

    // Ensure images are allocated
    if (!ensure_images(visible_w, needed_rows, visible_w, visible_h)) {
        return result;
    }

    // Update horizontal pass descriptor with current source texture
    const VkDescriptorImageInfo hor_sampler_info = {
        .sampler = source.sampler,
        .imageView = source.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    const VkDescriptorImageInfo hor_storage_info = {
        .imageView = inter_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    const VkWriteDescriptorSet hor_writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds_hor,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &hor_sampler_info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds_hor,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &hor_storage_info,
        },
    };
    vkUpdateDescriptorSets(VulkanCtx::self().get_device(), 2, hor_writes, 0,
                           nullptr);

    // --- Transition images ---

    // Intermediate: UNDEFINED → GENERAL (for compute write)
    // Output: UNDEFINED → GENERAL (for compute write in vertical pass)
    VkImageMemoryBarrier pre_barriers[] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = inter_image,
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
            .image = out_image,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        },
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 2, pre_barriers);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    // --- Horizontal pass: source → intermediate ---
    struct PushConstants {
        int32_t direction;
        float scale;
        int32_t src_size;
        float local_offset;
        int32_t src_start;
    };

    {
        PushConstants pc = {
            .direction = 0,
            .scale = scale,
            .src_size = static_cast<int32_t>(source.width),
            .local_offset = local_offset_x,
            .src_start = 0,
        };

        vkCmdPushConstants(cmd, pipe_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(pc), &pc);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                pipe_layout, 0, 1, &ds_hor, 0, nullptr);

        const uint32_t gx = (visible_w + 15) / 16;
        const uint32_t gy = (needed_rows + 15) / 16;
        vkCmdDispatch(cmd, gx, gy, 1);
    }

    // --- Barrier: intermediate write → read ---
    {
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = inter_image,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &barrier);
    }

    // --- Vertical pass: intermediate → output ---
    {
        PushConstants pc = {
            .direction = 1,
            .scale = scale,
            .src_size = static_cast<int32_t>(source.height),
            .local_offset = local_offset_y,
            .src_start = src_y_first,
        };

        vkCmdPushConstants(cmd, pipe_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(pc), &pc);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                pipe_layout, 0, 1, &ds_ver, 0, nullptr);

        const uint32_t gx = (visible_w + 15) / 16;
        const uint32_t gy = (visible_h + 15) / 16;
        vkCmdDispatch(cmd, gx, gy, 1);
    }

    // --- Transition output to SHADER_READ_ONLY for sampling ---
    {
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = out_image,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &barrier);
    }

    result_texture.width = visible_w;
    result_texture.height = visible_h;

    result.texture = &result_texture;
    result.x = vis_x;
    result.y = vis_y;
    result.w = static_cast<float>(visible_w);
    result.h = static_cast<float>(visible_h);

    return result;
}

#endif // HAVE_VULKAN
