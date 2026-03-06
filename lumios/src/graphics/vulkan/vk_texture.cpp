#include "vk_texture.h"
#include "vk_init.h"
#include "vk_buffer.h"
#include <stb_image.h>

namespace lumios {

void transition_image_layout(VkCommandBuffer cmd, VkImage image,
                             VkImageLayout old_layout, VkImageLayout new_layout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = old_layout;
    barrier.newLayout           = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount   = 1;
    barrier.subresourceRange.layerCount   = 1;

    VkPipelineStageFlags src_stage, dst_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);
}

GPUTexture create_texture_from_data(VulkanContext& ctx, VkCommandPool pool,
                                    const u8* pixels, u32 width, u32 height, u32 channels) {
    GPUTexture tex;
    tex.width  = width;
    tex.height = height;

    VkDeviceSize img_size = width * height * 4;

    // Staging buffer
    GPUBuffer staging = create_buffer(ctx.allocator, img_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* mapped;
    vmaMapMemory(ctx.allocator, staging.allocation, &mapped);
    if (channels == 4) {
        memcpy(mapped, pixels, img_size);
    } else {
        auto* dst = static_cast<u8*>(mapped);
        for (u32 i = 0; i < width * height; i++) {
            dst[i*4+0] = channels > 0 ? pixels[i*channels+0] : 255;
            dst[i*4+1] = channels > 1 ? pixels[i*channels+1] : 255;
            dst[i*4+2] = channels > 2 ? pixels[i*channels+2] : 255;
            dst[i*4+3] = 255;
        }
    }
    vmaUnmapMemory(ctx.allocator, staging.allocation);

    // Image
    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = VK_FORMAT_R8G8B8A8_SRGB;
    ici.extent        = {width, height, 1};
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VK_CHECK(vmaCreateImage(ctx.allocator, &ici, &aci, &tex.image, &tex.allocation, nullptr));

    // Copy staging -> image
    VkCommandBuffer cmd = ctx.begin_single_command(pool);
    transition_image_layout(cmd, tex.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, staging.buffer, tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    transition_image_layout(cmd, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    ctx.end_single_command(pool, cmd);

    destroy_buffer(ctx.allocator, staging);

    // Image view
    VkImageViewCreateInfo vi{};
    vi.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image    = tex.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format   = VK_FORMAT_R8G8B8A8_SRGB;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(ctx.device, &vi, nullptr, &tex.view));

    // Sampler
    VkSamplerCreateInfo si{};
    si.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter    = VK_FILTER_LINEAR;
    si.minFilter    = VK_FILTER_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.anisotropyEnable = VK_TRUE;
    si.maxAnisotropy    = ctx.device_properties.limits.maxSamplerAnisotropy;
    si.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VK_CHECK(vkCreateSampler(ctx.device, &si, nullptr, &tex.sampler));

    return tex;
}

GPUTexture load_texture_from_file(VulkanContext& ctx, VkCommandPool pool, const std::string& path) {
    int w, h, ch;
    u8* pixels = stbi_load(path.c_str(), &w, &h, &ch, STBI_rgb_alpha);
    if (!pixels) {
        LOG_ERROR("Failed to load texture: %s", path.c_str());
        return create_default_white_texture(ctx, pool);
    }

    GPUTexture tex = create_texture_from_data(ctx, pool, pixels, w, h, 4);
    stbi_image_free(pixels);
    LOG_INFO("Loaded texture: %s (%dx%d)", path.c_str(), w, h);
    return tex;
}

GPUTexture create_default_white_texture(VulkanContext& ctx, VkCommandPool pool) {
    u8 white[] = {255, 255, 255, 255};
    return create_texture_from_data(ctx, pool, white, 1, 1, 4);
}

void destroy_texture(VulkanContext& ctx, GPUTexture& tex) {
    if (tex.sampler) { vkDestroySampler(ctx.device, tex.sampler, nullptr); tex.sampler = VK_NULL_HANDLE; }
    if (tex.view)    { vkDestroyImageView(ctx.device, tex.view, nullptr); tex.view = VK_NULL_HANDLE; }
    if (tex.image)   { vmaDestroyImage(ctx.allocator, tex.image, tex.allocation); tex.image = VK_NULL_HANDLE; }
}

} // namespace lumios
