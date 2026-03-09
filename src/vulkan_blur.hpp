// SPDX-License-Identifier: MIT
// GPU blur for background effects (extend/mirror modes).
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "buildconf.hpp"

#ifdef HAVE_VULKAN

#include "vulkan_texture.hpp"

#include <vulkan/vulkan.h>

class VulkanBlur {
public:
    /**
     * Get global instance.
     * @return blur instance
     */
    static VulkanBlur& self();

    /**
     * Initialize blur resources.
     * @param width window width
     * @param height window height
     * @return true on success
     */
    bool init(uint32_t width, uint32_t height);

    /**
     * Destroy all resources.
     */
    void destroy();

    /**
     * Resize offscreen images for new window dimensions.
     * @param width new width
     * @param height new height
     * @return true on success
     */
    bool resize(uint32_t width, uint32_t height);

    /**
     * Prepare blur source by blitting source image scaled-to-fill.
     * @param cmd command buffer (before render pass)
     * @param source source texture to blur
     */
    void prepare(VkCommandBuffer cmd, const GpuTexture& source);

    /**
     * Apply multi-pass box blur.
     * @param cmd command buffer (before render pass)
     */
    void apply(VkCommandBuffer cmd);

    /**
     * Get blurred result as a GpuTexture for drawing with draw_image().
     * @return result texture reference
     */
    const GpuTexture& get_result() const { return result_texture; }

    /**
     * Check if blur is initialized.
     * @return true if ready
     */
    bool is_valid() const { return image_a != VK_NULL_HANDLE; }

private:
    bool create_images(uint32_t width, uint32_t height);
    bool create_compute_pipeline();
    bool create_descriptors();
    void destroy_images();

    // Ping-pong offscreen images (R8G8B8A8_UNORM for rgba8 shader compat)
    VkImage image_a = VK_NULL_HANDLE;
    VkImage image_b = VK_NULL_HANDLE;
    VkDeviceMemory memory_a = VK_NULL_HANDLE;
    VkDeviceMemory memory_b = VK_NULL_HANDLE;
    VkImageView view_a = VK_NULL_HANDLE;
    VkImageView view_b = VK_NULL_HANDLE;

    // Compute pipeline
    VkDescriptorSetLayout compute_ds_layout = VK_NULL_HANDLE;
    VkPipelineLayout compute_pipe_layout = VK_NULL_HANDLE;
    VkPipeline blur_pipeline = VK_NULL_HANDLE;

    // Descriptor sets for ping-pong
    VkDescriptorSet ds_a_to_b = VK_NULL_HANDLE;
    VkDescriptorSet ds_b_to_a = VK_NULL_HANDLE;

    // Result presented as GpuTexture for draw_image() compatibility
    GpuTexture result_texture;

    uint32_t img_width = 0;
    uint32_t img_height = 0;

    // Blur passes matching CPU: sigma=16, 3 passes, radii [15, 16, 16]
    static constexpr int BLUR_PASSES = 3;
    static constexpr int blur_radii[BLUR_PASSES] = { 15, 16, 16 };
};

#endif // HAVE_VULKAN
