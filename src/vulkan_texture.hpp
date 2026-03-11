// SPDX-License-Identifier: MIT
// GPU texture management and caching.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "buildconf.hpp"

#ifdef HAVE_VULKAN

#include "pixmap.hpp"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <list>
#include <map>
#include <utility>

/** GPU texture representing a decoded image on the GPU. */
struct GpuTexture {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSet descriptor = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    size_t memory_size = 0;

    void destroy();
};

/** Cache key for texture lookup. */
using TextureCacheKey = std::pair<size_t, size_t>; // (source_id, frame_index)

/** LRU texture cache with VRAM budget. */
class TextureCache {
public:
    /**
     * Set VRAM budget.
     * @param budget maximum GPU memory in bytes
     */
    void set_budget(size_t budget) { vram_budget = budget; }

    /**
     * Get or create a GPU texture from a Pixmap.
     * @param pm source pixmap
     * @param source_id image identifier
     * @param frame_index animation frame index
     * @return pointer to cached texture, or nullptr if VRAM exhausted
     */
    GpuTexture* get_or_upload(const Pixmap& pm, size_t source_id,
                              size_t frame_index);

    /**
     * Get a cached texture without uploading.
     * @param source_id image identifier
     * @param frame_index animation frame index
     * @return pointer to cached texture, or nullptr if not cached
     */
    GpuTexture* get(size_t source_id, size_t frame_index);

    /**
     * Remove all textures for a given source.
     * @param source_id image identifier
     */
    void release(size_t source_id);

    /**
     * Clear all cached textures.
     */
    void clear();

private:
    bool upload(GpuTexture& tex, const Pixmap& pm);
    void evict_lru();

    std::map<TextureCacheKey, GpuTexture> cache;
    std::list<TextureCacheKey> lru_order;
    std::vector<GpuTexture> pending_destroy; ///< Deferred destruction queue
    size_t vram_budget = 256 * 1024 * 1024; // 256MB default
    size_t vram_used = 0;
    static constexpr size_t MAX_ENTRIES = 512; ///< Max cached textures (descriptor pool limit)

public:
    /**
     * Destroy textures queued for deferred destruction.
     * Call after GPU fence signals (previous frame complete).
     */
    void flush_pending();
};

#endif // HAVE_VULKAN
