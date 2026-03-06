#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "../../core/types.h"
#include "../../core/log.h"

namespace lumios {

constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

#define VK_CHECK(expr)                                              \
    do {                                                            \
        VkResult _r = (expr);                                       \
        if (_r != VK_SUCCESS) {                                     \
            LOG_ERROR("Vulkan error %d at %s:%d", _r, __FILE__, __LINE__); \
        }                                                           \
    } while(0)

struct GPUBuffer {
    VkBuffer      buffer     = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize  size       = 0;
};

struct GPUTexture {
    VkImage       image      = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView   view       = VK_NULL_HANDLE;
    VkSampler     sampler    = VK_NULL_HANDLE;
    u32           width = 0, height = 0;
};

struct GPUMesh {
    GPUBuffer vertex_buffer;
    GPUBuffer index_buffer;
    u32 vertex_count = 0;
    u32 index_count  = 0;
};

struct GPUMaterial {
    GPUBuffer        ubo;
    VkDescriptorSet  descriptor = VK_NULL_HANDLE;
};

} // namespace lumios
