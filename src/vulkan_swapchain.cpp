// SPDX-License-Identifier: MIT
// Vulkan swapchain management and frame presentation.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "buildconf.hpp"

#ifdef HAVE_VULKAN

#include "vulkan_ctx.hpp"
#include "vulkan_swapchain.hpp"

#include <vulkan/vulkan_wayland.h>

#include <algorithm>
#include <cstdio>

VulkanSwapchain::~VulkanSwapchain()
{
    destroy();
}

bool VulkanSwapchain::init(struct wl_display* wl_display,
                           struct wl_surface* wl_surface, uint32_t width,
                           uint32_t height)
{
    auto& ctx = VulkanCtx::self();

    // Create Wayland surface
    const VkWaylandSurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = wl_display,
        .surface = wl_surface,
    };

    if (vkCreateWaylandSurfaceKHR(ctx.get_instance(), &surface_info, nullptr,
                                  &surface) != VK_SUCCESS) {
        return false;
    }

    // Verify surface support
    VkBool32 present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(ctx.get_physical_device(),
                                         ctx.get_graphics_family(), surface,
                                         &present_support);
    if (!present_support) {
        return false;
    }

    if (!create_swapchain(width, height)) {
        return false;
    }

    if (!create_sync_objects()) {
        return false;
    }

    if (!create_command_buffer()) {
        return false;
    }

    return true;
}

void VulkanSwapchain::destroy()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    if (dev != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(dev);

        if (in_flight != VK_NULL_HANDLE) {
            vkDestroyFence(dev, in_flight, nullptr);
            in_flight = VK_NULL_HANDLE;
        }
        if (render_finished != VK_NULL_HANDLE) {
            vkDestroySemaphore(dev, render_finished, nullptr);
            render_finished = VK_NULL_HANDLE;
        }
        if (image_available != VK_NULL_HANDLE) {
            vkDestroySemaphore(dev, image_available, nullptr);
            image_available = VK_NULL_HANDLE;
        }

        destroy_swapchain();
    }

    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(ctx.get_instance(), surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
}

bool VulkanSwapchain::recreate(uint32_t width, uint32_t height)
{
    auto& ctx = VulkanCtx::self();
    vkDeviceWaitIdle(ctx.get_device());

    destroy_swapchain();
    return create_swapchain(width, height);
}

bool VulkanSwapchain::create_swapchain(uint32_t width, uint32_t height)
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    // Get surface capabilities
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.get_physical_device(),
                                              surface, &caps);

    // Choose extent
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width = std::clamp(width, caps.minImageExtent.width,
                                  caps.maxImageExtent.width);
        extent.height = std::clamp(height, caps.minImageExtent.height,
                                   caps.maxImageExtent.height);
    }

    // Choose image count (prefer triple buffering)
    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    // Choose present mode (prefer MAILBOX for low latency)
    uint32_t mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.get_physical_device(),
                                              surface, &mode_count, nullptr);
    std::vector<VkPresentModeKHR> modes(mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        ctx.get_physical_device(), surface, &mode_count, modes.data());

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = mode;
            break;
        }
    }

    const VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = format,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    if (vkCreateSwapchainKHR(dev, &swapchain_info, nullptr, &swapchain) !=
        VK_SUCCESS) {
        return false;
    }

    // Get swapchain images
    vkGetSwapchainImagesKHR(dev, swapchain, &image_count, nullptr);
    images.resize(image_count);
    vkGetSwapchainImagesKHR(dev, swapchain, &image_count, images.data());

    if (!create_image_views()) {
        return false;
    }

    return create_framebuffers();
}

void VulkanSwapchain::destroy_swapchain()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    for (auto fb : framebuffers) {
        vkDestroyFramebuffer(dev, fb, nullptr);
    }
    framebuffers.clear();

    for (auto iv : image_views) {
        vkDestroyImageView(dev, iv, nullptr);
    }
    image_views.clear();

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(dev, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    images.clear();
}

bool VulkanSwapchain::create_image_views()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    image_views.resize(images.size());
    for (size_t i = 0; i < images.size(); ++i) {
        const VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .components = { VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        if (vkCreateImageView(dev, &view_info, nullptr, &image_views[i]) !=
            VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

bool VulkanSwapchain::create_framebuffers()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    framebuffers.resize(image_views.size());
    for (size_t i = 0; i < image_views.size(); ++i) {
        const VkFramebufferCreateInfo fb_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = ctx.get_render_pass(),
            .attachmentCount = 1,
            .pAttachments = &image_views[i],
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };

        if (vkCreateFramebuffer(dev, &fb_info, nullptr, &framebuffers[i]) !=
            VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

bool VulkanSwapchain::create_sync_objects()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    const VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    const VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    return vkCreateSemaphore(dev, &sem_info, nullptr, &image_available) ==
               VK_SUCCESS &&
           vkCreateSemaphore(dev, &sem_info, nullptr, &render_finished) ==
               VK_SUCCESS &&
           vkCreateFence(dev, &fence_info, nullptr, &in_flight) == VK_SUCCESS;
}

bool VulkanSwapchain::create_command_buffer()
{
    auto& ctx = VulkanCtx::self();

    const VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx.get_command_pool(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    return vkAllocateCommandBuffers(ctx.get_device(), &alloc_info,
                                    &command_buffer) == VK_SUCCESS;
}

VkFramebuffer VulkanSwapchain::get_framebuffer() const
{
    return framebuffers[current_image];
}

VkCommandBuffer VulkanSwapchain::begin_frame()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    // Wait for previous frame
    vkWaitForFences(dev, 1, &in_flight, VK_TRUE, UINT64_MAX);

    // Acquire next image
    VkResult result = vkAcquireNextImageKHR(dev, swapchain, UINT64_MAX,
                                            image_available, VK_NULL_HANDLE,
                                            &current_image);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return VK_NULL_HANDLE; // Need recreation
    }

    vkResetFences(dev, 1, &in_flight);
    vkResetCommandBuffer(command_buffer, 0);

    // Begin command buffer (render pass started separately)
    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

void VulkanSwapchain::begin_render_pass()
{
    auto& ctx = VulkanCtx::self();

    const VkClearValue clear_color = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    const VkRenderPassBeginInfo rp_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx.get_render_pass(),
        .framebuffer = framebuffers[current_image],
        .renderArea = { .offset = { 0, 0 }, .extent = extent },
        .clearValueCount = 1,
        .pClearValues = &clear_color,
    };
    vkCmdBeginRenderPass(command_buffer, &rp_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor
    const VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    const VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent = extent,
    };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

bool VulkanSwapchain::end_frame()
{
    auto& ctx = VulkanCtx::self();

    vkCmdEndRenderPass(command_buffer);
    vkEndCommandBuffer(command_buffer);

    const VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &image_available,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &render_finished,
    };

    if (vkQueueSubmit(ctx.get_graphics_queue(), 1, &submit_info, in_flight) !=
        VK_SUCCESS) {
        return false;
    }

    const VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render_finished,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &current_image,
    };

    VkResult result = vkQueuePresentKHR(ctx.get_graphics_queue(), &present_info);

    return result != VK_ERROR_OUT_OF_DATE_KHR &&
           result != VK_SUBOPTIMAL_KHR;
}

#endif // HAVE_VULKAN
