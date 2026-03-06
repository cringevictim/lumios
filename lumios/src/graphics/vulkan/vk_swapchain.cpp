#include "vk_swapchain.h"
#include "vk_init.h"
#include <algorithm>

namespace lumios {

static VkSurfaceFormatKHR choose_format(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    return formats[0];
}

static VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& modes) {
    for (auto m : modes)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& cap, u32 w, u32 h) {
    if (cap.currentExtent.width != UINT32_MAX) return cap.currentExtent;
    VkExtent2D e{w, h};
    e.width  = std::clamp(e.width,  cap.minImageExtent.width,  cap.maxImageExtent.width);
    e.height = std::clamp(e.height, cap.minImageExtent.height, cap.maxImageExtent.height);
    return e;
}

bool VulkanSwapchain::create(VulkanContext& ctx, u32 width, u32 height,
                             VkSwapchainKHR old, VkSurfaceKHR surface_override) {
    VkSurfaceKHR surface = surface_override ? surface_override : ctx.surface;

    VkSurfaceCapabilitiesKHR cap;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physical_device, surface, &cap);

    u32 fmt_count, pm_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, surface, &fmt_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmt_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, surface, &fmt_count, formats.data());

    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, surface, &pm_count, nullptr);
    std::vector<VkPresentModeKHR> modes(pm_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, surface, &pm_count, modes.data());

    auto format = choose_format(formats);
    auto mode   = choose_present_mode(modes);
    extent       = choose_extent(cap, width, height);
    image_format = format.format;

    u32 img_count = cap.minImageCount + 1;
    if (cap.maxImageCount > 0 && img_count > cap.maxImageCount)
        img_count = cap.maxImageCount;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = surface;
    ci.minImageCount    = img_count;
    ci.imageFormat      = format.format;
    ci.imageColorSpace  = format.colorSpace;
    ci.imageExtent      = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.preTransform     = cap.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = mode;
    ci.clipped          = VK_TRUE;
    ci.oldSwapchain     = old;

    u32 families[] = {ctx.graphics_family, ctx.present_family};
    if (ctx.graphics_family != ctx.present_family) {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = families;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VK_CHECK(vkCreateSwapchainKHR(ctx.device, &ci, nullptr, &handle));

    vkGetSwapchainImagesKHR(ctx.device, handle, &img_count, nullptr);
    images.resize(img_count);
    vkGetSwapchainImagesKHR(ctx.device, handle, &img_count, images.data());

    image_views.resize(img_count);
    for (u32 i = 0; i < img_count; i++) {
        VkImageViewCreateInfo vi{};
        vi.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image    = images[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format   = image_format;
        vi.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.baseMipLevel   = 0;
        vi.subresourceRange.levelCount     = 1;
        vi.subresourceRange.baseArrayLayer = 0;
        vi.subresourceRange.layerCount     = 1;
        VK_CHECK(vkCreateImageView(ctx.device, &vi, nullptr, &image_views[i]));
    }

    if (!create_depth_resources(ctx)) return false;

    LOG_INFO("Swapchain created: %ux%u, %u images", extent.width, extent.height, img_count);
    return true;
}

bool VulkanSwapchain::create_depth_resources(VulkanContext& ctx) {
    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = depth_format;
    ici.extent        = {extent.width, extent.height, 1};
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    aci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VK_CHECK(vmaCreateImage(ctx.allocator, &ici, &aci, &depth_image, &depth_allocation, nullptr));

    VkImageViewCreateInfo vi{};
    vi.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image    = depth_image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format   = depth_format;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(ctx.device, &vi, nullptr, &depth_view));
    return true;
}

void VulkanSwapchain::cleanup(VulkanContext& ctx) {
    if (depth_view)       { vkDestroyImageView(ctx.device, depth_view, nullptr); depth_view = VK_NULL_HANDLE; }
    if (depth_image)      { vmaDestroyImage(ctx.allocator, depth_image, depth_allocation); depth_image = VK_NULL_HANDLE; }
    for (auto v : image_views) vkDestroyImageView(ctx.device, v, nullptr);
    image_views.clear();
    images.clear();
    if (handle) { vkDestroySwapchainKHR(ctx.device, handle, nullptr); handle = VK_NULL_HANDLE; }
}

} // namespace lumios
