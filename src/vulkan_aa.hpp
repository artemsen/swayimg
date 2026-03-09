// SPDX-License-Identifier: MIT
// GPU-accelerated MKS13 anti-aliased image scaling.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "buildconf.hpp"

#ifdef HAVE_VULKAN

#include "vulkan_texture.hpp"

#include <vulkan/vulkan.h>

class VulkanAA {
public:
    /** AA result with position for rendering. */
    struct Result {
        const GpuTexture* texture = nullptr;
        float x = 0; // screen position
        float y = 0;
        float w = 0; // display size
        float h = 0;
    };

    /**
     * Get global instance.
     * @return AA instance
     */
    static VulkanAA& self();

    /**
     * Initialize compute pipeline.
     * @return true on success
     */
    bool init();

    /**
     * Destroy all resources.
     */
    void destroy();

    /**
     * Apply MKS13 anti-aliased scaling.
     * Must be called before render pass (compute work).
     * @param cmd command buffer
     * @param source source texture
     * @param scale image scale factor
     * @param pos_x image X position on screen
     * @param pos_y image Y position on screen
     * @param wnd_w window width
     * @param wnd_h window height
     * @return result with texture and screen position, or nullptr texture
     */
    Result apply(VkCommandBuffer cmd, const GpuTexture& source, float scale,
                 float pos_x, float pos_y, uint32_t wnd_w, uint32_t wnd_h);

    /**
     * Check if AA pipeline is ready.
     * @return true if initialized
     */
    bool is_valid() const { return pipeline != VK_NULL_HANDLE; }

private:
    bool create_pipeline();
    bool ensure_images(uint32_t inter_w, uint32_t inter_h, uint32_t out_w,
                       uint32_t out_h);
    void destroy_images();

    // Compute pipeline
    VkDescriptorSetLayout ds_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipe_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    // Intermediate image (after horizontal pass)
    VkImage inter_image = VK_NULL_HANDLE;
    VkDeviceMemory inter_memory = VK_NULL_HANDLE;
    VkImageView inter_view = VK_NULL_HANDLE;
    VkSampler inter_sampler = VK_NULL_HANDLE;
    uint32_t inter_w = 0, inter_h = 0;

    // Output image (after vertical pass)
    VkImage out_image = VK_NULL_HANDLE;
    VkDeviceMemory out_memory = VK_NULL_HANDLE;
    VkImageView out_view = VK_NULL_HANDLE;
    uint32_t out_w = 0, out_h = 0;

    // Descriptor sets (updated per-apply)
    VkDescriptorSet ds_hor = VK_NULL_HANDLE; // source → intermediate
    VkDescriptorSet ds_ver = VK_NULL_HANDLE; // intermediate → output

    // Result presented as GpuTexture for draw_image()
    GpuTexture result_texture;
};

#endif // HAVE_VULKAN
