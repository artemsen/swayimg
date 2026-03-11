// SPDX-License-Identifier: MIT
// Vulkan rendering context: instance, device, and resource management.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "buildconf.hpp"

#ifdef HAVE_VULKAN

#include "vulkan_ctx.hpp"

#include <vulkan/vulkan_wayland.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

VulkanCtx& VulkanCtx::self()
{
    static VulkanCtx ctx;
    return ctx;
}

bool VulkanCtx::init(struct wl_display* wl_display,
                     struct wl_surface* wl_surface, uint32_t width,
                     uint32_t height)
{
    (void)wl_display;
    (void)wl_surface;
    (void)width;
    (void)height;

    if (!create_instance()) {
        return false;
    }

#ifndef NDEBUG
    setup_debug_messenger();
#endif

    if (!select_physical_device()) {
        destroy();
        return false;
    }

    if (!create_device()) {
        destroy();
        return false;
    }

    if (!create_render_pass()) {
        destroy();
        return false;
    }

    if (!create_command_pool()) {
        destroy();
        return false;
    }

    if (!create_descriptor_pool()) {
        destroy();
        return false;
    }

    return true;
}

void VulkanCtx::destroy()
{
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);

        if (descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
            descriptor_pool = VK_NULL_HANDLE;
        }
        if (command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, command_pool, nullptr);
            command_pool = VK_NULL_HANDLE;
        }
        if (render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, render_pass, nullptr);
            render_pass = VK_NULL_HANDLE;
        }

        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

#ifndef NDEBUG
    destroy_debug_messenger();
#endif

    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }

    physical_device = VK_NULL_HANDLE;
    graphics_queue = VK_NULL_HANDLE;
    transfer_queue = VK_NULL_HANDLE;
    graphics_family = UINT32_MAX;
    transfer_family = UINT32_MAX;
}

bool VulkanCtx::create_instance()
{
    const VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "swayimg",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "swayimg",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
    };

    std::vector<const char*> layers;

#ifndef NDEBUG
    // Check for validation layer support
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    const char* validation_layer = "VK_LAYER_KHRONOS_validation";
    for (const auto& layer : available_layers) {
        if (strcmp(layer.layerName, validation_layer) == 0) {
            layers.push_back(validation_layer);
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            break;
        }
    }
#endif

    const VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    return vkCreateInstance(&create_info, nullptr, &instance) == VK_SUCCESS;
}

bool VulkanCtx::select_physical_device()
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (device_count == 0) {
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    // Prefer discrete GPU, fall back to any GPU with graphics support
    VkPhysicalDevice fallback = VK_NULL_HANDLE;

    for (const auto& dev : devices) {
        // Check for swapchain extension support
        uint32_t ext_count = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count,
                                             nullptr);
        std::vector<VkExtensionProperties> exts(ext_count);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count,
                                             exts.data());

        bool has_swapchain = false;
        for (const auto& ext : exts) {
            if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) ==
                0) {
                has_swapchain = true;
                break;
            }
        }
        if (!has_swapchain) {
            continue;
        }

        // Check for graphics queue family
        uint32_t qf_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, nullptr);
        std::vector<VkQueueFamilyProperties> qf_props(qf_count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count,
                                                 qf_props.data());

        bool has_graphics = false;
        for (uint32_t i = 0; i < qf_count; ++i) {
            if (qf_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                has_graphics = true;
                break;
            }
        }
        if (!has_graphics) {
            continue;
        }

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physical_device = dev;
            return true;
        }

        if (fallback == VK_NULL_HANDLE) {
            fallback = dev;
        }
    }

    if (fallback != VK_NULL_HANDLE) {
        physical_device = fallback;
        return true;
    }

    return false;
}

bool VulkanCtx::create_device()
{
    // Find queue families
    uint32_t qf_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &qf_count,
                                             nullptr);
    std::vector<VkQueueFamilyProperties> qf_props(qf_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &qf_count,
                                             qf_props.data());

    for (uint32_t i = 0; i < qf_count; ++i) {
        if ((qf_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            graphics_family == UINT32_MAX) {
            graphics_family = i;
        } else if ((qf_props[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                   !(qf_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                   transfer_family == UINT32_MAX) {
            transfer_family = i;
        }
    }

    if (graphics_family == UINT32_MAX) {
        return false;
    }

    const float queue_priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_infos;

    queue_infos.push_back({
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    });

    if (transfer_family != UINT32_MAX) {
        queue_infos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = transfer_family,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        });
    }

    const char* device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    const VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size()),
        .pQueueCreateInfos = queue_infos.data(),
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_extensions,
    };

    if (vkCreateDevice(physical_device, &device_info, nullptr, &device) !=
        VK_SUCCESS) {
        return false;
    }

    vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
    if (transfer_family != UINT32_MAX) {
        vkGetDeviceQueue(device, transfer_family, 0, &transfer_queue);
    } else {
        transfer_queue = graphics_queue;
        transfer_family = graphics_family;
    }

    return true;
}

bool VulkanCtx::create_render_pass()
{
    const VkAttachmentDescription color_attachment = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    const VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    const VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };

    const VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    const VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    return vkCreateRenderPass(device, &render_pass_info, nullptr,
                              &render_pass) == VK_SUCCESS;
}

bool VulkanCtx::create_command_pool()
{
    const VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics_family,
    };

    return vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) ==
           VK_SUCCESS;
}

bool VulkanCtx::create_descriptor_pool()
{
    const VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2048 },  // Audit R1: increased from 1024 to support 4GB VRAM budget
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 },
    };

    const VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 2048,  // Audit R1: increased from 1024
        .poolSizeCount = 3,
        .pPoolSizes = pool_sizes,
    };

    return vkCreateDescriptorPool(device, &pool_info, nullptr,
                                  &descriptor_pool) == VK_SUCCESS;
}

VkCommandBuffer VulkanCtx::begin_single_command()
{
    const VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &alloc_info, &cmd);

    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &begin_info);

    return cmd;
}

void VulkanCtx::end_single_command(VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);

    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };

    vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue);

    vkFreeCommandBuffers(device, command_pool, 1, &cmd);
}

uint32_t VulkanCtx::find_memory_type(uint32_t type_filter,
                                     VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }

    return UINT32_MAX;
}

size_t VulkanCtx::get_vram_budget() const
{
    if (physical_device == VK_NULL_HANDLE) {
        return 256 * 1024 * 1024; // 256MB default
    }

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    // Find largest device-local heap
    VkDeviceSize max_heap = 0;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if (mem_props.memoryTypes[i].propertyFlags &
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            const uint32_t heap_idx = mem_props.memoryTypes[i].heapIndex;
            max_heap = std::max(max_heap, mem_props.memoryHeaps[heap_idx].size);
        }
    }

    // 50% of device-local heap, capped at 4GB
    constexpr size_t MAX_BUDGET = 4UL * 1024UL * 1024UL * 1024UL;
    const size_t budget = static_cast<size_t>(max_heap / 2);
    return std::min(budget, MAX_BUDGET);
}

#ifndef NDEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
               VkDebugUtilsMessageTypeFlagsEXT /*type*/,
               const VkDebugUtilsMessengerCallbackDataEXT* data,
               void* /*user_data*/)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        fprintf(stderr, "Vulkan: %s\n", data->pMessage);
    }
    return VK_FALSE;
}

bool VulkanCtx::setup_debug_messenger()
{
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!func) {
        return false;
    }

    const VkDebugUtilsMessengerCreateInfoEXT create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
    };

    return func(instance, &create_info, nullptr, &debug_messenger) ==
           VK_SUCCESS;
}

void VulkanCtx::destroy_debug_messenger()
{
    if (debug_messenger != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance,
                                  "vkDestroyDebugUtilsMessengerEXT"));
        if (func) {
            func(instance, debug_messenger, nullptr);
        }
        debug_messenger = VK_NULL_HANDLE;
    }
}
#endif // NDEBUG

#endif // HAVE_VULKAN
