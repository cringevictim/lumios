#pragma once

#include "vk_common.h"

namespace lumios {

struct VulkanContext;

GPUBuffer create_buffer(VmaAllocator allocator, VkDeviceSize size,
                        VkBufferUsageFlags usage, VmaMemoryUsage mem_usage);

void destroy_buffer(VmaAllocator allocator, GPUBuffer& buf);

void upload_buffer_data(VmaAllocator allocator, GPUBuffer& buf, const void* data, VkDeviceSize size);

void upload_to_gpu(VulkanContext& ctx, VkCommandPool pool,
                   GPUBuffer& dst, const void* data, VkDeviceSize size);

} // namespace lumios
