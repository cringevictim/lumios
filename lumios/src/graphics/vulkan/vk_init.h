#pragma once

#include "vk_common.h"

struct GLFWwindow;

namespace lumios {

struct VulkanContext {
    VkInstance               instance        = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    VkSurfaceKHR             surface         = VK_NULL_HANDLE;
    VkPhysicalDevice         physical_device = VK_NULL_HANDLE;
    VkDevice                 device          = VK_NULL_HANDLE;
    VmaAllocator             allocator       = VK_NULL_HANDLE;

    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue  = VK_NULL_HANDLE;
    u32     graphics_family = 0;
    u32     present_family  = 0;

    VkPhysicalDeviceProperties device_properties{};

    bool init(GLFWwindow* window);
    void shutdown();

    VkCommandBuffer begin_single_command(VkCommandPool pool);
    void end_single_command(VkCommandPool pool, VkCommandBuffer cmd);
};

} // namespace lumios
