// SPDX-License-Identifier: MIT
// Vulkan rendering context: instance, device, and resource management.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "buildconf.hpp"

#ifdef HAVE_VULKAN

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <vector>

class VulkanCtx {
public:
    /**
     * Get global instance.
     * @return context instance
     */
    static VulkanCtx& self();

    /**
     * Initialize Vulkan context.
     * @param wl_display Wayland display handle
     * @param wl_surface Wayland surface handle
     * @param width initial window width
     * @param height initial window height
     * @return true if initialization succeeded
     */
    bool init(struct wl_display* wl_display, struct wl_surface* wl_surface,
              uint32_t width, uint32_t height);

    /**
     * Destroy all Vulkan resources.
     */
    void destroy();

    /**
     * Check if Vulkan is initialized and ready.
     * @return true if Vulkan is active
     */
    bool is_active() const { return device != VK_NULL_HANDLE; }

    /**
     * Get the logical device.
     * @return Vulkan device handle
     */
    VkDevice get_device() const { return device; }

    /**
     * Get the physical device.
     * @return Vulkan physical device handle
     */
    VkPhysicalDevice get_physical_device() const { return physical_device; }

    /**
     * Get the graphics queue.
     * @return Vulkan queue handle
     */
    VkQueue get_graphics_queue() const { return graphics_queue; }

    /**
     * Get the graphics queue family index.
     * @return queue family index
     */
    uint32_t get_graphics_family() const { return graphics_family; }

    /**
     * Get the render pass.
     * @return render pass handle
     */
    VkRenderPass get_render_pass() const { return render_pass; }

    /**
     * Get the command pool.
     * @return command pool handle
     */
    VkCommandPool get_command_pool() const { return command_pool; }

    /**
     * Get the descriptor pool.
     * @return descriptor pool handle
     */
    VkDescriptorPool get_descriptor_pool() const { return descriptor_pool; }

    /**
     * Get the Vulkan instance.
     * @return instance handle
     */
    VkInstance get_instance() const { return instance; }

    /**
     * Allocate a one-time command buffer and begin recording.
     * @return command buffer handle
     */
    VkCommandBuffer begin_single_command();

    /**
     * End recording, submit, and wait for a one-time command buffer.
     * @param cmd command buffer to submit
     */
    void end_single_command(VkCommandBuffer cmd);

    /**
     * Find a suitable memory type index.
     * @param type_filter bitmask of suitable memory types
     * @param properties required memory properties
     * @return memory type index, or UINT32_MAX on failure
     */
    uint32_t find_memory_type(uint32_t type_filter,
                              VkMemoryPropertyFlags properties) const;

    /**
     * Get recommended VRAM budget for texture cache.
     * @return budget in bytes (50% of device-local heap, max 512MB)
     */
    size_t get_vram_budget() const;

private:
    bool create_instance();
    bool select_physical_device();
    bool create_device();
    bool create_render_pass();
    bool create_command_pool();
    bool create_descriptor_pool();

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue transfer_queue = VK_NULL_HANDLE;
    uint32_t graphics_family = UINT32_MAX;
    uint32_t transfer_family = UINT32_MAX;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    bool setup_debug_messenger();
    void destroy_debug_messenger();
#endif
};

#endif // HAVE_VULKAN
