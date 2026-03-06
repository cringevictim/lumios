#pragma once

#include "vk_common.h"
#include <string>

namespace lumios {

struct VulkanContext;

GPUTexture create_texture_from_data(VulkanContext& ctx, VkCommandPool pool,
                                    const u8* pixels, u32 width, u32 height, u32 channels);

GPUTexture load_texture_from_file(VulkanContext& ctx, VkCommandPool pool, const std::string& path);

GPUTexture create_default_white_texture(VulkanContext& ctx, VkCommandPool pool);

void destroy_texture(VulkanContext& ctx, GPUTexture& tex);

void transition_image_layout(VkCommandBuffer cmd, VkImage image,
                             VkImageLayout old_layout, VkImageLayout new_layout);

} // namespace lumios
