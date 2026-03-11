// SPDX-License-Identifier: MIT
// GPU texture management and caching.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "buildconf.hpp"

#ifdef HAVE_VULKAN

#include "vulkan_ctx.hpp"
#include "vulkan_pipeline.hpp"
#include "vulkan_texture.hpp"

#include <algorithm>
#include <cstring>

void GpuTexture::destroy()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    if (dev == VK_NULL_HANDLE) {
        return;
    }

    if (descriptor != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(dev, ctx.get_descriptor_pool(), 1, &descriptor);
        descriptor = VK_NULL_HANDLE;
    }
    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }
    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, view, nullptr);
        view = VK_NULL_HANDLE;
    }
    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(dev, image, nullptr);
        image = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(dev, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

GpuTexture* TextureCache::get_or_upload(const Pixmap& pm, size_t source_id,
                                        size_t frame_index)
{
    TextureCacheKey key = { source_id, frame_index };

    // Check cache first
    auto it = cache.find(key);
    if (it != cache.end()) {
        // Move to front of LRU
        lru_order.remove(key);
        lru_order.push_front(key);
        return &it->second;
    }

    // Calculate required memory
    size_t required = static_cast<size_t>(pm.width()) * pm.height() * 4;

    // Evict until enough space (VRAM budget and entry count)
    while ((vram_used + required > vram_budget || cache.size() >= MAX_ENTRIES) &&
           !lru_order.empty()) {
        evict_lru();
    }

    // If single image exceeds budget, signal failure
    if (required > vram_budget) {
        return nullptr;
    }

    // Create and upload
    GpuTexture tex;
    tex.width = pm.width();
    tex.height = pm.height();

    if (!upload(tex, pm)) {
        tex.destroy();
        return nullptr;
    }

    tex.memory_size = required;
    vram_used += required;

    cache[key] = tex;
    lru_order.push_front(key);

    return &cache[key];
}

GpuTexture* TextureCache::get(size_t source_id, size_t frame_index)
{
    TextureCacheKey key = { source_id, frame_index };
    auto it = cache.find(key);
    if (it != cache.end()) {
        lru_order.remove(key);
        lru_order.push_front(key);
        return &it->second;
    }
    return nullptr;
}

void TextureCache::release(size_t source_id)
{
    auto it = cache.begin();
    while (it != cache.end()) {
        if (it->first.first == source_id) {
            vram_used -= it->second.memory_size;
            pending_destroy.push_back(it->second);
            lru_order.remove(it->first);
            it = cache.erase(it);
        } else {
            ++it;
        }
    }
}

void TextureCache::clear()
{
    for (auto& [key, tex] : cache) {
        tex.destroy();
    }
    cache.clear();
    lru_order.clear();
    vram_used = 0;
}

void TextureCache::evict_lru()
{
    if (lru_order.empty()) {
        return;
    }

    auto key = lru_order.back();
    lru_order.pop_back();

    auto it = cache.find(key);
    if (it != cache.end()) {
        vram_used -= it->second.memory_size;
        pending_destroy.push_back(it->second);
        cache.erase(it);
    }
}

void TextureCache::flush_pending()
{
    for (auto& tex : pending_destroy) {
        tex.destroy();
    }
    pending_destroy.clear();
}

bool TextureCache::upload(GpuTexture& tex, const Pixmap& pm)
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    // Determine format
    switch (pm.format()) {
    case Pixmap::ARGB:
        tex.format = VK_FORMAT_B8G8R8A8_UNORM;
        break;
    case Pixmap::RGB:
        tex.format = VK_FORMAT_B8G8R8A8_UNORM; // Expand during copy
        break;
    case Pixmap::GS:
        tex.format = VK_FORMAT_R8_UNORM;
        break;
    default:
        return false;
    }

    // Create image
    const VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = tex.format,
        .extent = { tex.width, tex.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage =
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if (vkCreateImage(dev, &image_info, nullptr, &tex.image) != VK_SUCCESS) {
        return false;
    }

    // Allocate memory
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(dev, tex.image, &mem_reqs);

    uint32_t mem_type = ctx.find_memory_type(
        mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_type == UINT32_MAX) {
        return false;
    }

    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_type,
    };

    if (vkAllocateMemory(dev, &alloc_info, nullptr, &tex.memory) !=
        VK_SUCCESS) {
        return false;
    }
    vkBindImageMemory(dev, tex.image, tex.memory, 0);

    // Create staging buffer
    const size_t buffer_size =
        static_cast<size_t>(tex.width) * tex.height * 4;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;

    const VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = buffer_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (vkCreateBuffer(dev, &buf_info, nullptr, &staging_buffer) !=
        VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements staging_reqs;
    vkGetBufferMemoryRequirements(dev, staging_buffer, &staging_reqs);

    uint32_t staging_mem_type = ctx.find_memory_type(
        staging_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (staging_mem_type == UINT32_MAX) {
        vkDestroyBuffer(dev, staging_buffer, nullptr);
        return false;
    }

    const VkMemoryAllocateInfo staging_alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = staging_reqs.size,
        .memoryTypeIndex = staging_mem_type,
    };

    if (vkAllocateMemory(dev, &staging_alloc, nullptr, &staging_memory) !=
        VK_SUCCESS) {
        vkDestroyBuffer(dev, staging_buffer, nullptr);
        return false;
    }
    vkBindBufferMemory(dev, staging_buffer, staging_memory, 0);

    // Copy pixel data to staging buffer
    void* mapped = nullptr;
    vkMapMemory(dev, staging_memory, 0, buffer_size, 0, &mapped);

    if (pm.format() == Pixmap::GS) {
        // Grayscale: copy row by row (handle stride)
        auto* dst = static_cast<uint8_t*>(mapped);
        for (size_t y = 0; y < tex.height; ++y) {
            memcpy(dst + y * tex.width,
                   static_cast<const uint8_t*>(pm.ptr(0, y)), tex.width);
        }
    } else if (pm.format() == Pixmap::RGB) {
        // RGB stored as 4 bytes per pixel (BGRA layout), set alpha to 255
        auto* dst = static_cast<uint8_t*>(mapped);
        for (size_t y = 0; y < tex.height; ++y) {
            const auto* src =
                static_cast<const uint8_t*>(pm.ptr(0, y));
            for (size_t x = 0; x < tex.width; ++x) {
                const size_t di = (y * tex.width + x) * 4;
                const size_t si = x * 4;
                dst[di + 0] = src[si + 0]; // B
                dst[di + 1] = src[si + 1]; // G
                dst[di + 2] = src[si + 2]; // R
                dst[di + 3] = 255;         // A = opaque
            }
        }
    } else {
        // ARGB: copy row by row (handle stride)
        auto* dst = static_cast<uint8_t*>(mapped);
        const size_t row_bytes = static_cast<size_t>(tex.width) * 4;
        for (size_t y = 0; y < tex.height; ++y) {
            memcpy(dst + y * row_bytes,
                   static_cast<const uint8_t*>(pm.ptr(0, y)), row_bytes);
        }
    }

    vkUnmapMemory(dev, staging_memory);

    // Transfer to GPU
    VkCommandBuffer cmd = ctx.begin_single_command();

    // Transition to transfer dst
    const VkImageMemoryBarrier barrier_to_transfer = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = tex.image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier_to_transfer);

    // Copy buffer to image
    const VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { tex.width, tex.height, 1 },
    };
    vkCmdCopyBufferToImage(cmd, staging_buffer, tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to shader read
    const VkImageMemoryBarrier barrier_to_shader = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = tex.image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier_to_shader);

    ctx.end_single_command(cmd);

    // Cleanup staging
    vkDestroyBuffer(dev, staging_buffer, nullptr);
    vkFreeMemory(dev, staging_memory, nullptr);

    // Create image view
    const VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = tex.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = tex.format,
        .components = { VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY },
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    };

    if (vkCreateImageView(dev, &view_info, nullptr, &tex.view) != VK_SUCCESS) {
        return false;
    }

    // Create sampler (nearest-neighbor)
    const VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };

    if (vkCreateSampler(dev, &sampler_info, nullptr, &tex.sampler) !=
        VK_SUCCESS) {
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetLayout layout = VulkanPipeline::self().get_texture_layout();
    const VkDescriptorSetAllocateInfo ds_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ctx.get_descriptor_pool(),
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkResult result = vkAllocateDescriptorSets(dev, &ds_alloc, &tex.descriptor);

    // Audit R3: Handle descriptor pool exhaustion gracefully
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY) {
        // Descriptor pool exhausted, try evicting one texture and retrying
        evict_lru();
        result = vkAllocateDescriptorSets(dev, &ds_alloc, &tex.descriptor);
    }

    if (result != VK_SUCCESS) {
        // Allocation failed after retry, fallback to software rendering
        return false;
    }

    // Update descriptor set with texture
    const VkDescriptorImageInfo img_desc = {
        .sampler = tex.sampler,
        .imageView = tex.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    const VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = tex.descriptor,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &img_desc,
    };

    vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);

    return true;
}

#endif // HAVE_VULKAN
