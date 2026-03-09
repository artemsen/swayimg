// SPDX-License-Identifier: MIT
// Vulkan rendering pipelines for image display and compositing.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "buildconf.hpp"

#ifdef HAVE_VULKAN

#include "color.hpp"
#include "vulkan_texture.hpp"

#include <vulkan/vulkan.h>

/** Push constants for textured quad rendering (image pipeline). */
struct QuadPushConstants {
    float pos[2];      // position in pixels
    float size[2];     // size in pixels
    float viewport[2]; // viewport size in pixels
};

/** Push constants for solid color fill. */
struct FillPushConstants {
    float pos[2];      // position in pixels
    float size[2];     // size in pixels
    float viewport[2]; // viewport size in pixels
    float _pad[2];     // padding for vec4 alignment
    float color[4];    // RGBA color (at offset 32)
};

/** Push constants for checkerboard grid. */
struct GridPushConstants {
    float pos[2];      // position in pixels
    float size[2];     // size in pixels
    float viewport[2]; // viewport size in pixels
    float _pad[2];     // padding for vec4 alignment
    float color1[4];   // first grid color RGBA (at offset 32)
    float color2[4];   // second grid color RGBA (at offset 48)
    float cell_size;   // grid cell size in pixels (at offset 64)
};

class VulkanPipeline {
public:
    /**
     * Get global instance.
     * @return pipeline instance
     */
    static VulkanPipeline& self();

    /**
     * Initialize pipelines.
     * @param extent swapchain extent for viewport
     * @return true on success
     */
    bool init(VkExtent2D extent);

    /**
     * Destroy all pipelines.
     */
    void destroy();

    /**
     * Update viewport dimensions.
     * @param extent new swapchain extent
     */
    void update_viewport(VkExtent2D extent) { viewport = extent; }

    /**
     * Draw a textured quad.
     * @param cmd command buffer
     * @param texture GPU texture to draw
     * @param x,y position in pixels
     * @param width,height display size in pixels
     */
    void draw_image(VkCommandBuffer cmd, const GpuTexture& texture, float x,
                    float y, float width, float height);

    /**
     * Draw a solid color filled rectangle.
     * @param cmd command buffer
     * @param x,y position in pixels
     * @param width,height size in pixels
     * @param color fill color
     */
    void draw_fill(VkCommandBuffer cmd, float x, float y, float width,
                   float height, const argb_t& color);

    /**
     * Fill the area outside a rectangle with a solid color.
     * @param cmd command buffer
     * @param img_x,img_y image rectangle position
     * @param img_w,img_h image rectangle size
     * @param color fill color
     */
    void draw_fill_inverse(VkCommandBuffer cmd, float img_x, float img_y,
                           float img_w, float img_h, const argb_t& color);

    /**
     * Draw a checkerboard grid rectangle.
     * @param cmd command buffer
     * @param x,y position in pixels
     * @param width,height size in pixels
     * @param cell_size grid cell size in pixels
     * @param color1,color2 alternating grid colors
     */
    void draw_grid(VkCommandBuffer cmd, float x, float y, float width,
                   float height, float cell_size, const argb_t& color1,
                   const argb_t& color2);

    /**
     * Get descriptor set layout for texture binding.
     * @return descriptor set layout
     */
    VkDescriptorSetLayout get_texture_layout() const
    {
        return texture_layout;
    }

private:
    bool create_descriptor_layout();
    bool create_pipeline_layout();
    bool create_solid_layout();
    bool create_image_pipeline();
    bool create_fill_pipeline();
    bool create_grid_pipeline();
    VkShaderModule create_shader_module(const uint32_t* code, size_t size);

    /** Convert argb_t to RGBA float array. */
    static void argb_to_float(const argb_t& c, float out[4]);

    VkDescriptorSetLayout texture_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE; // image (textured)
    VkPipelineLayout solid_layout = VK_NULL_HANDLE;    // fill/grid (no texture)
    VkPipeline image_pipeline = VK_NULL_HANDLE;
    VkPipeline fill_pipeline = VK_NULL_HANDLE;
    VkPipeline grid_pipeline = VK_NULL_HANDLE;
    VkExtent2D viewport = {};
};

#endif // HAVE_VULKAN
