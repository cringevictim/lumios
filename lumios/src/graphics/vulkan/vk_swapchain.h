#pragma once

#include "vk_common.h"
#include <vector>

namespace lumios {

struct VulkanContext;

struct VulkanSwapchain {
    VkSwapchainKHR            handle = VK_NULL_HANDLE;
    VkFormat                  image_format{};
    VkExtent2D                extent{};
    std::vector<VkImage>      images;
    std::vector<VkImageView>  image_views;

    VkImage       depth_image      = VK_NULL_HANDLE;
    VmaAllocation depth_allocation = VK_NULL_HANDLE;
    VkImageView   depth_view       = VK_NULL_HANDLE;
    VkFormat      depth_format     = VK_FORMAT_D32_SFLOAT;

    bool create(VulkanContext& ctx, u32 width, u32 height,
                VkSwapchainKHR old = VK_NULL_HANDLE, VkSurfaceKHR surface_override = VK_NULL_HANDLE);
    void cleanup(VulkanContext& ctx);

private:
    bool create_depth_resources(VulkanContext& ctx);
};

} // namespace lumios
