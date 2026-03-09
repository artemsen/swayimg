// SPDX-License-Identifier: MIT
// Vulkan swapchain management and frame presentation.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "buildconf.hpp"

#ifdef HAVE_VULKAN

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

class VulkanSwapchain {
public:
    ~VulkanSwapchain();

    /**
     * Initialize swapchain.
     * @param wl_display Wayland display handle
     * @param wl_surface Wayland surface handle
     * @param width initial width
     * @param height initial height
     * @return true on success
     */
    bool init(struct wl_display* wl_display, struct wl_surface* wl_surface,
              uint32_t width, uint32_t height);

    /**
     * Destroy swapchain resources.
     */
    void destroy();

    /**
     * Recreate swapchain for new dimensions.
     * @param width new width
     * @param height new height
     * @return true on success
     */
    bool recreate(uint32_t width, uint32_t height);

    /**
     * Begin a new frame (acquire image, begin command buffer).
     * Compute/transfer work can be recorded before begin_render_pass().
     * @return command buffer for recording, or VK_NULL_HANDLE on failure
     */
    VkCommandBuffer begin_frame();

    /**
     * Begin the render pass (must be called after begin_frame).
     */
    void begin_render_pass();

    /**
     * End frame recording and present.
     * @return true on success, false if swapchain needs recreation
     */
    bool end_frame();

    /**
     * Get current framebuffer.
     * @return framebuffer for current swap image
     */
    VkFramebuffer get_framebuffer() const;

    /**
     * Get swapchain extent.
     * @return current swapchain dimensions
     */
    VkExtent2D get_extent() const { return extent; }

private:
    bool create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();
    bool create_image_views();
    bool create_framebuffers();
    bool create_sync_objects();
    bool create_command_buffer();

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkExtent2D extent = {};
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;

    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    std::vector<VkFramebuffer> framebuffers;

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore render_finished = VK_NULL_HANDLE;
    VkFence in_flight = VK_NULL_HANDLE;

    uint32_t current_image = 0;
};

#endif // HAVE_VULKAN
