// SPDX-License-Identifier: MIT
// Vulkan rendering pipelines for image display and compositing.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "buildconf.hpp"

#ifdef HAVE_VULKAN

#include "vulkan_ctx.hpp"
#include "vulkan_pipeline.hpp"

// Include compiled SPIR-V shaders
#include "fill_frag_spv.h"
#include "grid_frag_spv.h"
#include "image_frag_spv.h"
#include "quad_vert_spv.h"

#include <algorithm>

VulkanPipeline& VulkanPipeline::self()
{
    static VulkanPipeline pipeline;
    return pipeline;
}

bool VulkanPipeline::init(VkExtent2D extent)
{
    viewport = extent;

    if (!create_descriptor_layout()) {
        return false;
    }
    if (!create_pipeline_layout()) {
        return false;
    }
    if (!create_solid_layout()) {
        return false;
    }
    if (!create_image_pipeline()) {
        return false;
    }
    if (!create_fill_pipeline()) {
        return false;
    }
    if (!create_grid_pipeline()) {
        return false;
    }

    return true;
}

void VulkanPipeline::destroy()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    if (dev == VK_NULL_HANDLE) {
        return;
    }

    if (grid_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, grid_pipeline, nullptr);
        grid_pipeline = VK_NULL_HANDLE;
    }
    if (fill_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, fill_pipeline, nullptr);
        fill_pipeline = VK_NULL_HANDLE;
    }
    if (image_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, image_pipeline, nullptr);
        image_pipeline = VK_NULL_HANDLE;
    }
    if (solid_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, solid_layout, nullptr);
        solid_layout = VK_NULL_HANDLE;
    }
    if (pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, pipeline_layout, nullptr);
        pipeline_layout = VK_NULL_HANDLE;
    }
    if (texture_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(dev, texture_layout, nullptr);
        texture_layout = VK_NULL_HANDLE;
    }
}

bool VulkanPipeline::create_descriptor_layout()
{
    auto& ctx = VulkanCtx::self();

    const VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    const VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding,
    };

    return vkCreateDescriptorSetLayout(ctx.get_device(), &layout_info, nullptr,
                                       &texture_layout) == VK_SUCCESS;
}

bool VulkanPipeline::create_pipeline_layout()
{
    auto& ctx = VulkanCtx::self();

    const VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(QuadPushConstants),
    };

    const VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &texture_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    };

    return vkCreatePipelineLayout(ctx.get_device(), &layout_info, nullptr,
                                  &pipeline_layout) == VK_SUCCESS;
}

bool VulkanPipeline::create_solid_layout()
{
    auto& ctx = VulkanCtx::self();

    // Covers both fill (48 bytes) and grid (68 bytes) push constants
    const VkPushConstantRange push_range = {
        .stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(GridPushConstants), // max of fill/grid
    };

    const VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    };

    return vkCreatePipelineLayout(ctx.get_device(), &layout_info, nullptr,
                                  &solid_layout) == VK_SUCCESS;
}

VkShaderModule VulkanPipeline::create_shader_module(const uint32_t* code,
                                                    size_t size)
{
    auto& ctx = VulkanCtx::self();

    const VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = code,
    };

    VkShaderModule module;
    if (vkCreateShaderModule(ctx.get_device(), &create_info, nullptr,
                             &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

// Common pipeline state shared by all graphics pipelines
static VkGraphicsPipelineCreateInfo
make_base_pipeline_info(const VkPipelineShaderStageCreateInfo* stages,
                        const VkPipelineVertexInputStateCreateInfo* vi,
                        const VkPipelineInputAssemblyStateCreateInfo* ia,
                        const VkPipelineViewportStateCreateInfo* vp,
                        const VkPipelineRasterizationStateCreateInfo* rs,
                        const VkPipelineMultisampleStateCreateInfo* ms,
                        const VkPipelineColorBlendStateCreateInfo* cb,
                        const VkPipelineDynamicStateCreateInfo* dyn,
                        VkPipelineLayout layout, VkRenderPass render_pass)
{
    return {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = vi,
        .pInputAssemblyState = ia,
        .pViewportState = vp,
        .pRasterizationState = rs,
        .pMultisampleState = ms,
        .pColorBlendState = cb,
        .pDynamicState = dyn,
        .layout = layout,
        .renderPass = render_pass,
        .subpass = 0,
    };
}

bool VulkanPipeline::create_image_pipeline()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    VkShaderModule vert_module = create_shader_module(
        reinterpret_cast<const uint32_t*>(quad_vert_spv), quad_vert_spv_len);
    VkShaderModule frag_module = create_shader_module(
        reinterpret_cast<const uint32_t*>(image_frag_spv), image_frag_spv_len);

    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        if (vert_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(dev, vert_module, nullptr);
        }
        if (frag_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(dev, frag_module, nullptr);
        }
        return false;
    }

    const VkPipelineShaderStageCreateInfo stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module,
            .pName = "main",
        },
    };

    const VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };

    const VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    const VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    const VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    const VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };

    const VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    const VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = make_base_pipeline_info(
        stages, &vertex_input, &input_assembly, &viewport_state, &rasterizer,
        &multisampling, &color_blending, &dynamic_state, pipeline_layout,
        ctx.get_render_pass());

    VkResult result = vkCreateGraphicsPipelines(
        dev, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &image_pipeline);

    vkDestroyShaderModule(dev, vert_module, nullptr);
    vkDestroyShaderModule(dev, frag_module, nullptr);

    return result == VK_SUCCESS;
}

bool VulkanPipeline::create_fill_pipeline()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    VkShaderModule vert_module = create_shader_module(
        reinterpret_cast<const uint32_t*>(quad_vert_spv), quad_vert_spv_len);
    VkShaderModule frag_module = create_shader_module(
        reinterpret_cast<const uint32_t*>(fill_frag_spv), fill_frag_spv_len);

    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        if (vert_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(dev, vert_module, nullptr);
        }
        if (frag_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(dev, frag_module, nullptr);
        }
        return false;
    }

    const VkPipelineShaderStageCreateInfo stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module,
            .pName = "main",
        },
    };

    const VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };

    const VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    const VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    const VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    const VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };

    const VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    const VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = make_base_pipeline_info(
        stages, &vertex_input, &input_assembly, &viewport_state, &rasterizer,
        &multisampling, &color_blending, &dynamic_state, solid_layout,
        ctx.get_render_pass());

    VkResult result = vkCreateGraphicsPipelines(
        dev, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &fill_pipeline);

    vkDestroyShaderModule(dev, vert_module, nullptr);
    vkDestroyShaderModule(dev, frag_module, nullptr);

    return result == VK_SUCCESS;
}

bool VulkanPipeline::create_grid_pipeline()
{
    auto& ctx = VulkanCtx::self();
    VkDevice dev = ctx.get_device();

    VkShaderModule vert_module = create_shader_module(
        reinterpret_cast<const uint32_t*>(quad_vert_spv), quad_vert_spv_len);
    VkShaderModule frag_module = create_shader_module(
        reinterpret_cast<const uint32_t*>(grid_frag_spv), grid_frag_spv_len);

    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        if (vert_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(dev, vert_module, nullptr);
        }
        if (frag_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(dev, frag_module, nullptr);
        }
        return false;
    }

    const VkPipelineShaderStageCreateInfo stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module,
            .pName = "main",
        },
    };

    const VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };

    const VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    const VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    const VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    const VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };

    const VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    const VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = make_base_pipeline_info(
        stages, &vertex_input, &input_assembly, &viewport_state, &rasterizer,
        &multisampling, &color_blending, &dynamic_state, solid_layout,
        ctx.get_render_pass());

    VkResult result = vkCreateGraphicsPipelines(
        dev, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &grid_pipeline);

    vkDestroyShaderModule(dev, vert_module, nullptr);
    vkDestroyShaderModule(dev, frag_module, nullptr);

    return result == VK_SUCCESS;
}

void VulkanPipeline::argb_to_float(const argb_t& c, float out[4])
{
    out[0] = static_cast<float>(c.r) / 255.0f;
    out[1] = static_cast<float>(c.g) / 255.0f;
    out[2] = static_cast<float>(c.b) / 255.0f;
    out[3] = static_cast<float>(c.a) / 255.0f;
}

void VulkanPipeline::draw_image(VkCommandBuffer cmd, const GpuTexture& texture,
                                float x, float y, float width, float height)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, image_pipeline);

    if (texture.descriptor != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout, 0, 1, &texture.descriptor, 0,
                                nullptr);
    }

    const QuadPushConstants pc = {
        .pos = { x, y },
        .size = { width, height },
        .viewport = { static_cast<float>(viewport.width),
                      static_cast<float>(viewport.height) },
    };

    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(pc), &pc);

    vkCmdDraw(cmd, 4, 1, 0, 0);
}

void VulkanPipeline::draw_fill(VkCommandBuffer cmd, float x, float y,
                               float width, float height, const argb_t& color)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fill_pipeline);

    FillPushConstants pc = {
        .pos = { x, y },
        .size = { width, height },
        .viewport = { static_cast<float>(viewport.width),
                      static_cast<float>(viewport.height) },
        ._pad = {},
    };
    argb_to_float(color, pc.color);

    vkCmdPushConstants(cmd, solid_layout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);

    vkCmdDraw(cmd, 4, 1, 0, 0);
}

void VulkanPipeline::draw_fill_inverse(VkCommandBuffer cmd, float img_x,
                                       float img_y, float img_w, float img_h,
                                       const argb_t& color)
{
    const float wnd_w = static_cast<float>(viewport.width);
    const float wnd_h = static_cast<float>(viewport.height);

    // Top strip
    draw_fill(cmd, 0, 0, wnd_w, img_y, color);
    // Bottom strip
    draw_fill(cmd, 0, img_y + img_h, wnd_w, wnd_h - img_y - img_h, color);
    // Left strip
    draw_fill(cmd, 0, img_y, img_x, img_h, color);
    // Right strip
    draw_fill(cmd, img_x + img_w, img_y, wnd_w - img_x - img_w, img_h, color);
}

void VulkanPipeline::draw_grid(VkCommandBuffer cmd, float x, float y,
                               float width, float height, float cell_size,
                               const argb_t& color1, const argb_t& color2)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, grid_pipeline);

    GridPushConstants pc = {
        .pos = { x, y },
        .size = { width, height },
        .viewport = { static_cast<float>(viewport.width),
                      static_cast<float>(viewport.height) },
        ._pad = {},
        .cell_size = cell_size,
    };
    argb_to_float(color1, pc.color1);
    argb_to_float(color2, pc.color2);

    vkCmdPushConstants(cmd, solid_layout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);

    vkCmdDraw(cmd, 4, 1, 0, 0);
}

#endif // HAVE_VULKAN
